#include <windows.h>
#include <stdint.h>
#include <string.h>

#include "../ProviderBootstrap.h"

namespace
{
   char PATCHER_OWNER[] = "HD.Plugin.H3.NewSpellsFortitude";
   char PROVIDER_KEY[] = "HD.Plugin.H3.NewSpellsFortitude";
   char SPELL_KEY[] = "Fortitude";
   const int32_t FORTITUDE_SPELL_ID = 151;

   // Health percent per mastery: none, basic, advanced, expert.
   const int HEALTH_PERCENT[4] = {130, 130, 140, 150};

   // Battle stack fields of the SoD 3.2 executable used by this spell.
   const uint32_t STACK_ATTRIBUTES = 0x84;    // creature flags, 0x40 = war machine
   const uint32_t STACK_ORIG_HP = 0x6C;       // hit points of one creature before spells
   const uint32_t STACK_RESIDUAL = 0x58;      // damage already taken by the top creature
   const uint32_t STACK_HP = 0xC0;            // current hit points of one creature
   const uint32_t STACK_SIDE = 0xF4;
   const uint32_t STACK_INDEX = 0xF8;
   const uint32_t CREATURE_WAR_MACHINE = 0x40;

   // Spell table record: animation index at +8, effect per mastery at +0x34, 0x88 bytes each.
   const uint32_t SPELL_TABLE_POINTER = 0x687FA8;
   const uint32_t SPELL_RECORD_SIZE = 0x88;

   // Engine functions, __thiscall.
   const uint32_t STACK_APPLY_SPELL = 0x444610;        // (spell, duration, mastery, hero)
   const uint32_t MANAGER_PLAY_ANIMATION = 0x4963C0;  // (animation, stack, delay, showHit)
   const uint32_t MANAGER_REPORT_CAST = 0x5A8C60;     // (casterKind, spell, stack)

   template <class T> T& Field(void* object, uint32_t offset)
   {
      return *reinterpret_cast<T*>(static_cast<char*>(object) + offset);
   }

   bool ValidStatus(const NewSpellsStatusContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         context->spellId == FORTITUDE_SPELL_ID && context->stack &&
         context->mastery >= 0 && context->mastery <= 3;
   }

   bool ValidCombat(const NewSpellsCombatContextV1* context)
   {
      return context && context->size >= sizeof(*context) &&
         context->abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
         context->spellId == FORTITUDE_SPELL_ID && context->combatManager &&
         context->mastery >= 0 && context->mastery <= 3;
   }

   // Health of one creature with the spell's percent applied, the top creature keeps its wounds.
   void SetHealth(void* stack, int percent)
   {
      const int health = max(1, Field<int>(stack, STACK_ORIG_HP) * percent / 100);
      Field<int>(stack, STACK_HP) = health;
      if (health - 1 < Field<int>(stack, STACK_RESIDUAL))
         Field<int>(stack, STACK_RESIDUAL) = health - 1;
   }

   int32_t __stdcall ValidateTarget(NewSpellsCombatContextV1* context)
   {
      if (!ValidCombat(context) || !context->targetStack)
         return NEWSPELLS_PROVIDER_DENIED;
      if (Field<uint32_t>(context->targetStack, STACK_ATTRIBUTES) & CREATURE_WAR_MACHINE)
         return NEWSPELLS_PROVIDER_DENIED;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   // Applies the status through the engine (which calls OnStatusApply), plays the animation and
   // writes the log line. New Spells then runs the cast epilogue, so mana stays with the engine.
   int32_t __stdcall CastCombat(NewSpellsCombatContextV1* context)
   {
      if (ValidateTarget(context) != NEWSPELLS_PROVIDER_COMMITTED || context->spellPower <= 0)
         return NEWSPELLS_PROVIDER_DENIED;

      void* stack = context->targetStack;
      typedef int (__fastcall *ApplySpell)(void*, int, int, int, int, void*);
      typedef int (__fastcall *PlayAnimation)(void*, int, int, void*, int, int);
      typedef void (__fastcall *ReportCast)(void*, int, int, int, void*);

      reinterpret_cast<ApplySpell>(STACK_APPLY_SPELL)(stack, 0, FORTITUDE_SPELL_ID,
         context->spellPower, context->mastery, context->casterHero);

      const char* record = *reinterpret_cast<const char**>(SPELL_TABLE_POINTER) +
         FORTITUDE_SPELL_ID * SPELL_RECORD_SIZE;
      const int animation = *reinterpret_cast<const int*>(record + 8);
      reinterpret_cast<PlayAnimation>(MANAGER_PLAY_ANIMATION)(context->combatManager, 0,
         animation, stack, 100, 1);
      reinterpret_cast<ReportCast>(MANAGER_REPORT_CAST)(context->combatManager, 0,
         context->source == NEWSPELLS_SOURCE_CREATURE ? 1 : 0, FORTITUDE_SPELL_ID, stack);
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   int32_t __stdcall OnStatusApply(NewSpellsStatusContextV1* context)
   {
      if (!ValidStatus(context))
         return NEWSPELLS_PROVIDER_DENIED;
      SetHealth(context->stack, HEALTH_PERCENT[context->mastery]);
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   int32_t __stdcall OnStatusRemove(NewSpellsStatusContextV1* context)
   {
      if (!ValidStatus(context))
         return NEWSPELLS_PROVIDER_DENIED;
      SetHealth(context->stack, 100);
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   int32_t __stdcall OnCureOrDispel(NewSpellsStatusContextV1* context)
   {
      return ValidStatus(context) ? NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_DENIED;
   }

   // A rough value: the extra health as a share of the target's fighting strength.
   int32_t __stdcall EvaluateCombatAi(NewSpellsAiContextV1* context)
   {
      if (!context || context->size < sizeof(*context) ||
          context->abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
          context->spellId != FORTITUDE_SPELL_ID || !context->targetStack ||
          context->mastery < 0 || context->mastery > 3)
         return NEWSPELLS_PROVIDER_DENIED;

      void* stack = context->targetStack;
      context->score = 0;
      context->castNow = 0;
      if (Field<int>(stack, STACK_SIDE) != context->casterSide ||
          Field<int>(stack, 0x198 + 4 * FORTITUDE_SPELL_ID) > 0 ||
          (Field<uint32_t>(stack, STACK_ATTRIBUTES) & CREATURE_WAR_MACHINE))
         return NEWSPELLS_PROVIDER_COMMITTED;

      const int troops = Field<int>(stack, 0x4C);
      const int extraHealth = Field<int>(stack, STACK_ORIG_HP) * (HEALTH_PERCENT[context->mastery] - 100) / 100;
      context->score = troops * extraHealth * 5;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   const NewSpellsProviderSpellV1 FORTITUDE_SPELL =
   {
      sizeof(NewSpellsProviderSpellV1),
      NEWSPELLS_PROVIDER_ABI_VERSION_V1,
      FORTITUDE_SPELL_ID,
      NEWSPELLS_CAP_COMBAT_TARGET | NEWSPELLS_CAP_COMBAT_CAST | NEWSPELLS_CAP_STATUS_APPLY |
         NEWSPELLS_CAP_STATUS_REMOVE | NEWSPELLS_CAP_CURE_DISPEL | NEWSPELLS_CAP_COMBAT_AI,
      0,
      PROVIDER_KEY,
      SPELL_KEY,
      0,                 // ValidateAdventure
      0,                 // CastAdventure
      ValidateTarget,
      CastCombat,
      OnStatusApply,
      0,                 // OnStatusRound
      OnStatusRemove,
      OnCureOrDispel,
      0,                 // OnBattleLifecycle
      0,                 // OnCreatureCast
      0,                 // OnErmCast
      EvaluateCombatAi,
      0                  // EvaluateAdventureAi
   };

   const NewSpellsProviderBatchV1 FORTITUDE_BATCH =
   {
      sizeof(NewSpellsProviderBatchV1),
      NEWSPELLS_PROVIDER_ABI_VERSION_V1,
      PROVIDER_KEY,
      1,
      &FORTITUDE_SPELL
   };

   NewSpellsSdk::ProviderBootstrapV1 PROVIDER_BOOTSTRAP;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
   (void)reserved;
   if (reason != DLL_PROCESS_ATTACH)
      return TRUE;

   DisableThreadLibraryCalls(module);
   PROVIDER_BOOTSTRAP.Start(module, PATCHER_OWNER, &FORTITUDE_BATCH,
      NEWSPELLS_CAP_COMBAT_CAST | NEWSPELLS_CAP_STATUS_APPLY);
   return TRUE;
}
