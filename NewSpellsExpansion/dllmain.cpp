#include "stdafx.h"

#include "../NewSpells/ERA/era.h"
#include "../NewSpells/patcher_x86_commented.hpp"
#include "../NewSpells/NewSpellsProviderApi.h"
#include "Reinforcements.h"
#include "Blizzard.h"

#include <cstdint>

Patcher* _P = 0;
PatcherInstance* _PI = 0;

namespace
{
   char PATCHER_OWNER[] = "HD.Plugin.H3.NewSpellsExpansion";
   char PROVIDER_KEY[] = "HD.Plugin.H3.NewSpellsExpansion";
   char SPELL_KEY[] = "Reinforcements";
   char REGISTRY_VARIABLE[] = NEWSPELLS_PROVIDER_REGISTRY_VARIABLE_V1;
   const int REINFORCEMENTS_SPELL_ID = 96;
   bool providerActivated = false;
   bool batchAttempted = false;
   bool deferredAttempted = false;

   bool isExecutableAddress(const void* address)
   {
      if (!address)
         return false;

      MEMORY_BASIC_INFORMATION info = {};
      if (!VirtualQuery(address, &info, sizeof(info)) ||
          info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) ||
          (info.Protect & PAGE_NOACCESS))
         return false;

      const DWORD protection = info.Protect & 0xFF;
      return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
         protection == PAGE_EXECUTE_READWRITE ||
         protection == PAGE_EXECUTE_WRITECOPY;
   }

   int32_t __stdcall validateReinforcementsAdventure(
      NewSpellsAdventureContextV1* context)
   {
      if (!context || context->size < sizeof(*context) ||
          context->abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
          context->spellId != REINFORCEMENTS_SPELL_ID || !context->hero ||
          !context->isHuman || context->mastery < 0 || context->mastery > 3)
         return NEWSPELLS_PROVIDER_DENIED;
      if (!reinforcementNativeReady)
         return NEWSPELLS_PROVIDER_UNSUPPORTED;

      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   int32_t __stdcall castReinforcementsAdventure(
      NewSpellsAdventureContextV1* context)
   {
      if (validateReinforcementsAdventure(context) !=
          NEWSPELLS_PROVIDER_COMMITTED)
         return reinforcementNativeReady ? NEWSPELLS_PROVIDER_DENIED :
            NEWSPELLS_PROVIDER_UNSUPPORTED;

      const int result = castNativeReinforcements(
         reinterpret_cast<hero*>(context->hero), context->mastery);
      return result >= NEWSPELLS_PROVIDER_UNSUPPORTED &&
         result <= NEWSPELLS_PROVIDER_COMMITTED
         ? result : NEWSPELLS_PROVIDER_DENIED;
   }

   const NewSpellsProviderSpellV1 REINFORCEMENTS_DESCRIPTOR =
   {
      sizeof(NewSpellsProviderSpellV1),
      NEWSPELLS_PROVIDER_ABI_VERSION_V1,
      REINFORCEMENTS_SPELL_ID,
      NEWSPELLS_CAP_ADVENTURE_CAST,
      NEWSPELLS_PROVIDER_HUMAN_ONLY,
      PROVIDER_KEY,
      SPELL_KEY,
      validateReinforcementsAdventure,
      castReinforcementsAdventure,
      0, // ValidateCombatTarget
      0, // CastCombat
      0, // OnStatusApply
      0, // OnStatusRound
      0, // OnStatusRemove
      0, // OnCureOrDispel
      0, // OnBattleLifecycle
      0, // OnCreatureCast
      0, // OnErmCast
      0, // EvaluateCombatAi
      0  // EvaluateAdventureAi: the spell is intentionally human-only
   };

   NewSpellsProviderSpellV1 expansionSpells[2] = {};
   NewSpellsProviderBatchV1 expansionBatch =
   {
      sizeof(NewSpellsProviderBatchV1), NEWSPELLS_PROVIDER_ABI_VERSION_V1,
      PROVIDER_KEY, 0, expansionSpells
   };

   NewSpellsProviderRegistryV1* findProviderRegistry()
   {
      if (!_P)
         return 0;

      Variable* const variable = _P->VarFind(REGISTRY_VARIABLE);
      if (!variable || !variable->GetValue())
         return 0;

      NewSpellsProviderRegistryV1* const registry =
         reinterpret_cast<NewSpellsProviderRegistryV1*>(
            static_cast<std::uintptr_t>(variable->GetValue()));
      __try
      {
         if (registry->size < sizeof(NewSpellsProviderRegistryV1) ||
             registry->abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
             !(registry->capabilities & NEWSPELLS_CAP_ADVENTURE_CAST) ||
             !isExecutableAddress(reinterpret_cast<const void*>(
                registry->RegisterProviderBatch)) ||
             !isExecutableAddress(reinterpret_cast<const void*>(
                registry->IsRegistrationOpen)) ||
             !isExecutableAddress(reinterpret_cast<const void*>(
                registry->IsSpellRegistered)) ||
             !registry->IsRegistrationOpen())
            return 0;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         return 0;
      }

      return registry;
   }

   bool registerReinforcementsProvider(
      NewSpellsProviderRegistryV1* const registry)
   {
      if (!registry)
         return false;

      __try
      {
         batchAttempted = true;
         return registry->RegisterProviderBatch(&expansionBatch) != 0;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         return false;
      }
   }

   bool activateReinforcementsProvider(const bool finalAttempt)
   {
      if (providerActivated)
         return true;
      if (batchAttempted)
         return false;

      NewSpellsProviderRegistryV1* const registry = findProviderRegistry();
      if (!registry)
      {
         if (finalAttempt)
            Era::WriteLog("NewSpellsExpansion", "Provider registration",
               "New Spells provider registry v1 is missing, already closed, or lacks the adventure-cast capability.");
         return false;
      }

      expansionBatch.spellCount = 0;
      reinforcementNativeReady = initializeNativeReinforcements();
      if (reinforcementNativeReady && installNativeReinforcementLifecycleHandlers())
      {
         expansionSpells[expansionBatch.spellCount++] = REINFORCEMENTS_DESCRIPTOR;
         Era::RegisterHandler(saveNativeReinforcementState, "OnSavegameWrite");
         Era::RegisterHandler(loadNativeReinforcementState, "OnSavegameRead");
      }
      else
      {
         reinforcementNativeReady = false;
         shutdownNativeReinforcements();
      }
      if ((registry->capabilities & Blizzard::Capabilities) == Blizzard::Capabilities && Blizzard::Initialize(_P))
         expansionSpells[expansionBatch.spellCount++] = Blizzard::Descriptor(PROVIDER_KEY);
      if (!expansionBatch.spellCount || !registerReinforcementsProvider(registry))
      {
         shutdownNativeReinforcements();
         Blizzard::Shutdown();
         return false;
      }

      providerActivated = true;
      Era::WriteLog("NewSpellsExpansion", "Provider registration",
         "Registered available expansion spells (Reinforcements 96, Blizzard 97).");
      return true;
   }

   void __stdcall activateReinforcementsAfterWoG(Era::TEvent* event)
   {
      if (!providerActivated && !batchAttempted && !deferredAttempted)
      {
         deferredAttempted = true;
         activateReinforcementsProvider(true);
      }
   }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
   static bool initialized = false;
   if (reason != DLL_PROCESS_ATTACH || initialized)
      return TRUE;

   initialized = true;
   DisableThreadLibraryCalls(module);
   Era::ConnectEra(module, PATCHER_OWNER);

   _P = GetPatcher();
   if (!_P)
   {
      Era::WriteLog("NewSpellsExpansion", "Provider startup",
         "Patcher_x86 is unavailable; Reinforcements remains disabled.");
      return TRUE;
   }
   _PI = _P->CreateInstance(PATCHER_OWNER);
   if (!_PI)
   {
      Era::WriteLog("NewSpellsExpansion", "Provider startup",
         "Could not create the expansion patch owner; Reinforcements remains disabled.");
      return TRUE;
   }

   if (!activateReinforcementsProvider(false))
   {
      // ERA does not guarantee DLL process-attach order across different mod
      // directories. OnAfterWoG is the common rendezvous after every native
      // plugin has loaded but before New Spells seals provider registration.
      if (!Era::RegisterHandler)
      {
         Era::WriteLog("NewSpellsExpansion", "Provider registration",
            "ERA event registration is unavailable; Reinforcements remains disabled.");
         return TRUE;
      }
      __try
      {
         Era::RegisterHandler(activateReinforcementsAfterWoG, "OnAfterWoG");
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         Era::WriteLog("NewSpellsExpansion", "Provider registration",
            "Could not defer provider registration; Reinforcements remains disabled.");
         return TRUE;
      }
      Era::WriteLog("NewSpellsExpansion", "Provider registration",
         "New Spells has not published the registry yet; registration was deferred to OnAfterWoG.");
   }
   return TRUE;
}
