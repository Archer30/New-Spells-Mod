#include <windows.h>
#include <stdint.h>

#include "../ProviderBootstrap.h"

namespace
{
   char PATCHER_OWNER[] = "HD.Plugin.H3.NewSpellsSdkExample";
   char PROVIDER_KEY[] = "HD.Plugin.H3.NewSpellsSdkExample";
   char SPELL_KEY[] = "ExampleAdventure";
   const int32_t EXAMPLE_SPELL_ID = 97;

   bool ValidContext(const NewSpellsAdventureContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         context->spellId == EXAMPLE_SPELL_ID && context->hero &&
         context->isHuman && context->mastery >= 0 &&
         context->mastery <= 3;
   }

   int32_t __stdcall ValidateAdventure(
      NewSpellsAdventureContextV1* context)
   {
      return ValidContext(context) ? NEWSPELLS_PROVIDER_COMMITTED :
         NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall CastAdventure(NewSpellsAdventureContextV1* context)
   {
      if (!ValidContext(context))
         return NEWSPELLS_PROVIDER_DENIED;

      // Replace this safe placeholder with the provider-owned game effect.
      // Do not return COMMITTED until that effect has actually completed.
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }

   const NewSpellsProviderSpellV1 EXAMPLE_SPELL =
   {
      sizeof(NewSpellsProviderSpellV1),
      NEWSPELLS_PROVIDER_ABI_VERSION_V1,
      EXAMPLE_SPELL_ID,
      NEWSPELLS_CAP_ADVENTURE_CAST,
      NEWSPELLS_PROVIDER_HUMAN_ONLY,
      PROVIDER_KEY,
      SPELL_KEY,
      ValidateAdventure,
      CastAdventure,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
   };

   const NewSpellsProviderBatchV1 EXAMPLE_BATCH =
   {
      sizeof(NewSpellsProviderBatchV1),
      NEWSPELLS_PROVIDER_ABI_VERSION_V1,
      PROVIDER_KEY,
      1,
      &EXAMPLE_SPELL
   };

   NewSpellsSdk::ProviderBootstrapV1 PROVIDER_BOOTSTRAP;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
   (void)reserved;
   if (reason != DLL_PROCESS_ATTACH)
      return TRUE;

   DisableThreadLibraryCalls(module);
   PROVIDER_BOOTSTRAP.Start(module, PATCHER_OWNER, &EXAMPLE_BATCH,
      NEWSPELLS_CAP_ADVENTURE_CAST);
   return TRUE;
}
