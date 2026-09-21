#ifndef NEWSPELLS_SDK_PROVIDER_BOOTSTRAP_H
#define NEWSPELLS_SDK_PROVIDER_BOOTSTRAP_H

#include <windows.h>
#include <stdint.h>

#include "patcher_x86_commented.hpp"
#include "ERA/era.h"
#include <NewSpellsProviderApi.h>

namespace NewSpellsSdk
{
   class ProviderBootstrapV1;

   // One descriptor batch is registered per provider DLL. The examples include
   // this header from their single bootstrap translation unit, and keep both the
   // bootstrap and batch storage alive until process detach.
   inline ProviderBootstrapV1*& ProviderBootstrapSlot()
   {
      static ProviderBootstrapV1* bootstrap = 0;
      return bootstrap;
   }

   inline bool IsExecutableAddress(const void* address)
   {
      if (!address)
         return false;

      MEMORY_BASIC_INFORMATION information = {};
      if (!VirtualQuery(address, &information, sizeof(information)) ||
          information.State != MEM_COMMIT ||
          (information.Protect & PAGE_GUARD) ||
          (information.Protect & PAGE_NOACCESS))
         return false;

      const DWORD protection = information.Protect & 0xFF;
      return protection == PAGE_EXECUTE ||
         protection == PAGE_EXECUTE_READ ||
         protection == PAGE_EXECUTE_READWRITE ||
         protection == PAGE_EXECUTE_WRITECOPY;
   }

   inline NewSpellsProviderRegistryV1* FindOpenRegistry(
      Patcher* patcher, const uint32_t requiredCapabilities)
   {
      if (!patcher ||
          (requiredCapabilities & ~NEWSPELLS_CAP_ALL_V1) != 0)
         return 0;

      char registryVariable[] = NEWSPELLS_PROVIDER_REGISTRY_VARIABLE_V1;
      __try
      {
         Variable* const variable = patcher->VarFind(registryVariable);
         if (!variable || !variable->GetValue())
            return 0;

         NewSpellsProviderRegistryV1* const registry =
            reinterpret_cast<NewSpellsProviderRegistryV1*>(
               static_cast<uintptr_t>(variable->GetValue()));
         if (registry->size < sizeof(NewSpellsProviderRegistryV1) ||
             registry->abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
             (requiredCapabilities & ~registry->capabilities) != 0 ||
             !IsExecutableAddress(reinterpret_cast<const void*>(
                registry->RegisterProviderBatch)) ||
             !IsExecutableAddress(reinterpret_cast<const void*>(
                registry->IsRegistrationOpen)) ||
             !IsExecutableAddress(reinterpret_cast<const void*>(
                registry->IsSpellRegistered)) ||
             !registry->IsRegistrationOpen())
            return 0;

         return registry;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         return 0;
      }
   }

   inline bool RegisterProviderBatch(Patcher* patcher,
      const NewSpellsProviderBatchV1* batch,
      const uint32_t requiredCapabilities)
   {
      if (!batch || batch->size < sizeof(NewSpellsProviderBatchV1) ||
          batch->abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
          !batch->providerKey || !batch->spells || !batch->spellCount)
         return false;

      NewSpellsProviderRegistryV1* const registry =
         FindOpenRegistry(patcher, requiredCapabilities);
      if (!registry)
         return false;

      __try
      {
         return registry->RegisterProviderBatch(batch) != 0;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         return false;
      }
   }

   // Load-order-safe one-shot registration helper. Start() first tries the
   // already-published registry. If New Spells has not published it yet, the
   // helper registers exactly one OnAfterWoG attempt. A rejected or faulting
   // RegisterProviderBatch call is terminal and is never submitted again,
   // because a duplicate submission could contest otherwise valid spell IDs.
   class ProviderBootstrapV1
   {
   public:
      ProviderBootstrapV1() :
         patcher_(0),
         patcherInstance_(0),
         batch_(0),
         requiredCapabilities_(0),
         started_(false),
         registered_(false),
         terminal_(false),
         registrationInvoked_(false),
         deferredHandlerRegistered_(false),
         deferredAttempted_(false)
      {
      }

      bool Start(HMODULE module, char* patcherOwner,
         const NewSpellsProviderBatchV1* batch,
         const uint32_t requiredCapabilities)
      {
         if (started_)
            return registered_ || IsDeferred();

         started_ = true;
         if (!module || !patcherOwner || !*patcherOwner ||
             !IsValidBatch(batch) ||
             (requiredCapabilities & ~NEWSPELLS_CAP_ALL_V1) != 0)
         {
            terminal_ = true;
            return false;
         }

         ProviderBootstrapV1*& owner = ProviderBootstrapSlot();
         if (owner && owner != this)
         {
            terminal_ = true;
            return false;
         }
         owner = this;

         // Connect exactly once before using either ERA events or Patcher.
         // patcherOwner must have process lifetime and be unique to this DLL.
         Era::ConnectEra(module, patcherOwner);

         patcher_ = GetPatcher();
         if (!patcher_)
         {
            terminal_ = true;
            return false;
         }

         patcherInstance_ = patcher_->CreateInstance(patcherOwner);
         if (!patcherInstance_)
         {
            terminal_ = true;
            return false;
         }

         batch_ = batch;
         requiredCapabilities_ = requiredCapabilities;

         NewSpellsProviderRegistryV1* const registry =
            FindOpenRegistry(patcher_, requiredCapabilities_);
         if (registry)
            return InvokeRegistration(registry);

         if (!Era::RegisterHandler)
         {
            terminal_ = true;
            return false;
         }

         deferredHandlerRegistered_ = true;
         __try
         {
            Era::RegisterHandler(OnAfterWoG, "OnAfterWoG");
         }
         __except (EXCEPTION_EXECUTE_HANDLER)
         {
            deferredHandlerRegistered_ = false;
            terminal_ = true;
            return false;
         }
         return registered_ || IsDeferred();
      }

      bool IsRegistered() const
      {
         return registered_;
      }

      bool IsDeferred() const
      {
         return deferredHandlerRegistered_ && !deferredAttempted_ &&
            !terminal_;
      }

   private:
      ProviderBootstrapV1(const ProviderBootstrapV1&);
      ProviderBootstrapV1& operator=(const ProviderBootstrapV1&);

      static bool IsValidBatch(const NewSpellsProviderBatchV1* batch)
      {
         return batch && batch->size >= sizeof(NewSpellsProviderBatchV1) &&
            batch->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
            batch->providerKey && *batch->providerKey && batch->spells &&
            batch->spellCount;
      }

      bool InvokeRegistration(NewSpellsProviderRegistryV1* registry)
      {
         if (!registry || registrationInvoked_ || terminal_)
            return registered_;

         // Set this before entering provider-owned/core-owned callback code so
         // an exception or re-entrant event can never cause a duplicate batch.
         registrationInvoked_ = true;
         __try
         {
            registered_ = registry->RegisterProviderBatch(batch_) != 0;
         }
         __except (EXCEPTION_EXECUTE_HANDLER)
         {
            registered_ = false;
         }

         terminal_ = !registered_;
         return registered_;
      }

      void TryDeferredRegistration()
      {
         if (!deferredHandlerRegistered_ || deferredAttempted_ ||
             registrationInvoked_ || terminal_)
            return;

         // OnAfterWoG may be fired more than once during the process lifetime.
         // Consume this provider's sole deferred attempt before touching core.
         deferredAttempted_ = true;
         NewSpellsProviderRegistryV1* const registry =
            FindOpenRegistry(patcher_, requiredCapabilities_);
         if (registry)
            InvokeRegistration(registry);
         else
            terminal_ = true;
      }

      static void __stdcall OnAfterWoG(Era::TEvent* event)
      {
         (void)event;
         ProviderBootstrapV1* const bootstrap = ProviderBootstrapSlot();
         if (bootstrap)
            bootstrap->TryDeferredRegistration();
      }

      Patcher* patcher_;
      PatcherInstance* patcherInstance_;
      const NewSpellsProviderBatchV1* batch_;
      uint32_t requiredCapabilities_;
      bool started_;
      bool registered_;
      bool terminal_;
      bool registrationInvoked_;
      bool deferredHandlerRegistered_;
      bool deferredAttempted_;
   };
}

#endif
