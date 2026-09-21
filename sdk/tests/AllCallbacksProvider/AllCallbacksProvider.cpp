#include <windows.h>
#include <stdint.h>

#include "../../examples/ProviderBootstrap.h"

namespace
{
   char PATCHER_OWNER[] = "HD.Plugin.H3.NewSpellsSdkTest";
   char PROVIDER_KEY[] = "HD.Plugin.H3.NewSpellsSdkTest";
   char TARGETED_SPELL_KEY[] = "TargetedTimedStatus";
   char AREA_SPELL_KEY[] = "AreaDamage";
   char GLOBAL_SPELL_KEY[] = "GlobalHybrid";
   char SUMMON_SPELL_KEY[] = "Summon";

   const int32_t FIRST_TEST_SPELL_ID = 123;
   const int32_t LAST_TEST_SPELL_ID = 126;
   const int32_t TEST_SPELL_COUNT =
      LAST_TEST_SPELL_ID - FIRST_TEST_SPELL_ID + 1;
   const int32_t ADVENTURE_TEST_SPELL_ID = 125;

   const uint32_t TARGETED_CAPABILITIES =
      NEWSPELLS_CAP_COMBAT_TARGET |
      NEWSPELLS_CAP_COMBAT_CAST |
      NEWSPELLS_CAP_STATUS_APPLY |
      NEWSPELLS_CAP_STATUS_ROUND |
      NEWSPELLS_CAP_STATUS_REMOVE |
      NEWSPELLS_CAP_CURE_DISPEL |
      NEWSPELLS_CAP_BATTLE_LIFECYCLE |
      NEWSPELLS_CAP_CREATURE_CAST |
      NEWSPELLS_CAP_ERM_CAST |
      NEWSPELLS_CAP_COMBAT_AI;

   const uint32_t ROUTING_CAPABILITIES =
      NEWSPELLS_CAP_COMBAT_TARGET |
      NEWSPELLS_CAP_COMBAT_CAST |
      NEWSPELLS_CAP_BATTLE_LIFECYCLE |
      NEWSPELLS_CAP_CREATURE_CAST |
      NEWSPELLS_CAP_ERM_CAST |
      NEWSPELLS_CAP_COMBAT_AI;

   const uint32_t GLOBAL_CAPABILITIES =
      NEWSPELLS_CAP_ADVENTURE_CAST |
      NEWSPELLS_CAP_COMBAT_TARGET |
      NEWSPELLS_CAP_COMBAT_CAST |
      NEWSPELLS_CAP_BATTLE_LIFECYCLE |
      NEWSPELLS_CAP_COMBAT_AI |
      NEWSPELLS_CAP_ADVENTURE_AI;

   enum CallbackCounter
   {
      COUNTER_VALIDATE_ADVENTURE,
      COUNTER_CAST_ADVENTURE,
      COUNTER_VALIDATE_COMBAT,
      COUNTER_CAST_COMBAT,
      COUNTER_STATUS_APPLY,
      COUNTER_STATUS_ROUND,
      COUNTER_STATUS_REMOVE,
      COUNTER_CURE_DISPEL,
      COUNTER_BATTLE_LIFECYCLE,
      COUNTER_CREATURE_CAST,
      COUNTER_ERM_CAST,
      COUNTER_COMBAT_AI,
      COUNTER_ADVENTURE_AI,
      COUNTER_COUNT
   };

   volatile LONG callbackCounts[COUNTER_COUNT] = {};
   volatile LONG spellCallbackCounts[TEST_SPELL_COUNT][COUNTER_COUNT] = {};

   bool ValidTestSpellId(const int32_t spellId)
   {
      return spellId >= FIRST_TEST_SPELL_ID &&
         spellId <= LAST_TEST_SPELL_ID;
   }

   int SpellIndex(const int32_t spellId)
   {
      return ValidTestSpellId(spellId) ?
         spellId - FIRST_TEST_SPELL_ID : -1;
   }

   void Count(const CallbackCounter counter, const int32_t spellId)
   {
      InterlockedIncrement(&callbackCounts[counter]);
      const int index = SpellIndex(spellId);
      if (index >= 0)
         InterlockedIncrement(&spellCallbackCounts[index][counter]);
   }

   bool ValidAdventure(const NewSpellsAdventureContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         context->spellId == ADVENTURE_TEST_SPELL_ID;
   }

   bool ValidCombat(const NewSpellsCombatContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         ValidTestSpellId(context->spellId);
   }

   bool ValidStatus(const NewSpellsStatusContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         context->spellId == FIRST_TEST_SPELL_ID;
   }

   bool ValidBattle(const NewSpellsBattleContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         ValidTestSpellId(context->spellId) &&
         (context->event == NEWSPELLS_BATTLE_START ||
          context->event == NEWSPELLS_BATTLE_END);
   }

   bool ValidAi(const NewSpellsAiContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         ValidTestSpellId(context->spellId);
   }

   int32_t __stdcall ValidateAdventure(NewSpellsAdventureContextV1* context)
   {
      Count(COUNTER_VALIDATE_ADVENTURE, context ? context->spellId : -1);
      return ValidAdventure(context) ? NEWSPELLS_PROVIDER_COMMITTED :
         NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall CastAdventure(NewSpellsAdventureContextV1* context)
   {
      Count(COUNTER_CAST_ADVENTURE, context ? context->spellId : -1);
      return ValidAdventure(context) ? NEWSPELLS_PROVIDER_COMMITTED :
         NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall ValidateCombat(NewSpellsCombatContextV1* context)
   {
      Count(COUNTER_VALIDATE_COMBAT, context ? context->spellId : -1);
      return ValidCombat(context) ? NEWSPELLS_PROVIDER_COMMITTED :
         NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall CastCombat(NewSpellsCombatContextV1* context)
   {
      Count(COUNTER_CAST_COMBAT, context ? context->spellId : -1);
      return ValidCombat(context) ? NEWSPELLS_PROVIDER_COMMITTED :
         NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t StatusResult(NewSpellsStatusContextV1* context,
      const CallbackCounter counter, const int32_t expectedEvent)
   {
      Count(counter, context ? context->spellId : -1);
      return ValidStatus(context) && context->event == expectedEvent ?
         NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall StatusApply(NewSpellsStatusContextV1* context)
   {
      return StatusResult(context, COUNTER_STATUS_APPLY,
         NEWSPELLS_STATUS_APPLY);
   }

   int32_t __stdcall StatusRound(NewSpellsStatusContextV1* context)
   {
      return StatusResult(context, COUNTER_STATUS_ROUND,
         NEWSPELLS_STATUS_ROUND);
   }

   int32_t __stdcall StatusRemove(NewSpellsStatusContextV1* context)
   {
      return StatusResult(context, COUNTER_STATUS_REMOVE,
         NEWSPELLS_STATUS_REMOVE);
   }

   int32_t __stdcall CureDispel(NewSpellsStatusContextV1* context)
   {
      Count(COUNTER_CURE_DISPEL, context ? context->spellId : -1);
      return ValidStatus(context) &&
         (context->event == NEWSPELLS_STATUS_CURE ||
          context->event == NEWSPELLS_STATUS_DISPEL) ?
         NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall BattleLifecycle(NewSpellsBattleContextV1* context)
   {
      Count(COUNTER_BATTLE_LIFECYCLE, context ? context->spellId : -1);
      return ValidBattle(context) ? NEWSPELLS_PROVIDER_COMMITTED :
         NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall CreatureCast(NewSpellsCombatContextV1* context)
   {
      Count(COUNTER_CREATURE_CAST, context ? context->spellId : -1);
      return ValidCombat(context) &&
         context->source == NEWSPELLS_SOURCE_CREATURE ?
         NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall ErmCast(NewSpellsCombatContextV1* context)
   {
      Count(COUNTER_ERM_CAST, context ? context->spellId : -1);
      return ValidCombat(context) && context->source == NEWSPELLS_SOURCE_ERM ?
         NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_DENIED;
   }

   int32_t __stdcall CombatAi(NewSpellsAiContextV1* context)
   {
      Count(COUNTER_COMBAT_AI, context ? context->spellId : -1);
      if (!ValidAi(context))
         return NEWSPELLS_PROVIDER_DENIED;

      context->score = 1000 + context->spellId;
      context->targetHex = context->spellId - FIRST_TEST_SPELL_ID;
      context->castNow = 1;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   int32_t __stdcall AdventureAi(NewSpellsAiContextV1* context)
   {
      Count(COUNTER_ADVENTURE_AI, context ? context->spellId : -1);
      if (!ValidAi(context) || context->spellId != ADVENTURE_TEST_SPELL_ID)
         return NEWSPELLS_PROVIDER_DENIED;

      context->score = 1000 + context->spellId;
      context->targetHex = context->spellId - FIRST_TEST_SPELL_ID;
      context->castNow = 1;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   const NewSpellsProviderSpellV1 TEST_SPELLS[] =
   {
      {
         sizeof(NewSpellsProviderSpellV1),
         NEWSPELLS_PROVIDER_ABI_VERSION_V1,
         123,
         TARGETED_CAPABILITIES,
         0,
         PROVIDER_KEY,
         TARGETED_SPELL_KEY,
         0,
         0,
         ValidateCombat,
         CastCombat,
         StatusApply,
         StatusRound,
         StatusRemove,
         CureDispel,
         BattleLifecycle,
         CreatureCast,
         ErmCast,
         CombatAi,
         0
      },
      {
         sizeof(NewSpellsProviderSpellV1),
         NEWSPELLS_PROVIDER_ABI_VERSION_V1,
         124,
         ROUTING_CAPABILITIES,
         0,
         PROVIDER_KEY,
         AREA_SPELL_KEY,
         0,
         0,
         ValidateCombat,
         CastCombat,
         0,
         0,
         0,
         0,
         BattleLifecycle,
         CreatureCast,
         ErmCast,
         CombatAi,
         0
      },
      {
         sizeof(NewSpellsProviderSpellV1),
         NEWSPELLS_PROVIDER_ABI_VERSION_V1,
         125,
         GLOBAL_CAPABILITIES,
         0,
         PROVIDER_KEY,
         GLOBAL_SPELL_KEY,
         ValidateAdventure,
         CastAdventure,
         ValidateCombat,
         CastCombat,
         0,
         0,
         0,
         0,
         BattleLifecycle,
         0,
         0,
         CombatAi,
         AdventureAi
      },
      {
         sizeof(NewSpellsProviderSpellV1),
         NEWSPELLS_PROVIDER_ABI_VERSION_V1,
         126,
         ROUTING_CAPABILITIES,
         0,
         PROVIDER_KEY,
         SUMMON_SPELL_KEY,
         0,
         0,
         ValidateCombat,
         CastCombat,
         0,
         0,
         0,
         0,
         BattleLifecycle,
         CreatureCast,
         ErmCast,
         CombatAi,
         0
      }
   };

   const NewSpellsProviderBatchV1 TEST_BATCH =
   {
      sizeof(NewSpellsProviderBatchV1),
      NEWSPELLS_PROVIDER_ABI_VERSION_V1,
      PROVIDER_KEY,
      TEST_SPELL_COUNT,
      TEST_SPELLS
   };

   NewSpellsSdk::ProviderBootstrapV1 PROVIDER_BOOTSTRAP;
}

extern "C" __declspec(dllexport) LONG __stdcall
NewSpellsSdkTestGetCallbackCount(const int32_t counter)
{
   return counter >= 0 && counter < COUNTER_COUNT ?
      InterlockedCompareExchange(&callbackCounts[counter], 0, 0) : -1;
}

extern "C" __declspec(dllexport) LONG __stdcall
NewSpellsSdkTestGetSpellCallbackCount(const int32_t spellId,
   const int32_t counter)
{
   const int index = SpellIndex(spellId);
   return index >= 0 && counter >= 0 && counter < COUNTER_COUNT ?
      InterlockedCompareExchange(&spellCallbackCounts[index][counter], 0, 0) :
      -1;
}

extern "C" __declspec(dllexport) void __stdcall
NewSpellsSdkTestResetCallbackCounts()
{
   for (int counter = 0; counter < COUNTER_COUNT; ++counter)
      InterlockedExchange(&callbackCounts[counter], 0);

   for (int spell = 0; spell < TEST_SPELL_COUNT; ++spell)
   {
      for (int counter = 0; counter < COUNTER_COUNT; ++counter)
         InterlockedExchange(&spellCallbackCounts[spell][counter], 0);
   }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
   (void)reserved;
   if (reason != DLL_PROCESS_ATTACH)
      return TRUE;

   DisableThreadLibraryCalls(module);
   PROVIDER_BOOTSTRAP.Start(module, PATCHER_OWNER, &TEST_BATCH,
      NEWSPELLS_CAP_ALL_V1);
   return TRUE;
}
