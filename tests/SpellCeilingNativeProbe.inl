// Test-only startup probe in a disposable game copy, never packaged, runs after the exe init hook.
namespace CeilingProbe
{
   int checks = 0;
   const char* testName = "startup";

   void Check(bool condition, const char* expression, int line)
   {
      ++checks;
      if (!condition)
      {
         char text[512];
         sprintf_s(text, "FAIL [%s] %s (line %d, check %d)", testName, expression, line, checks);
         Era::WriteLog("Ceiling native probe", "Result", text);
         RaiseException(0xE0420214, 0, 0, 0);
      }
   }
#define CEILING_CHECK(x) CeilingProbe::Check(!!(x), #x, __LINE__)

   bool IsJump(int address)
   {
      return *reinterpret_cast<const unsigned char*>(address) == 0xE9;
   }

   // The exe's loader rewrites the name it is given, so it gets a copy.
   int DefFrames(const char* name)
   {
      char buffer[32];
      strcpy_s(buffer, sizeof(buffer), name);
      _Def_* def = _Def_::Load(buffer);
      if (!def)
         return -1;
      const int frames = def->groups_count > 0 && def->groups && def->groups[0]
         ? static_cast<int>(def->groups[0]->frames_count) : -2;
      def->DerefOrDestruct();
      return frames;
   }
}

void ceilingProbeTranslations(const char* stage)
{
   char text[256];
   const char* keys[] = {"era.spells.95.name", "NewSpells.Config.MaxSpellId", "era.spells.150.name", "NewSpells.DataSpells.150.kind"};
   for (int i = 0; i < 4; ++i)
   {
      char* value = Era::tr(keys[i]);
      sprintf_s(text, "%s: %s = %s", stage, keys[i], value ? value : "(null)");
      Era::WriteLog("Ceiling native probe", "Translation", text);
   }
}

void __stdcall ceilingProbeAfterPlugins(Era::TEvent* e) { ceilingProbeTranslations("OnAfterLoadEraPlugins"); }
void __stdcall ceilingProbeAfterWog(Era::TEvent* e) { ceilingProbeTranslations("OnAfterWoG"); }
void __stdcall ceilingProbeBeforeErm(Era::TEvent* e) { ceilingProbeTranslations("OnBeforeErm"); }

namespace CeilingProbe
{
   void CheckMageGuild()
   {
      testName = "mage-guild loop";
      LoHook* loop = (LoHook*)_P->GetLastPatchAt(0x5BEA6E);
      LoHook* compare = (LoHook*)_P->GetLastPatchAt(0x5BEA2A);
      CEILING_CHECK(loop && compare);
      HookContext context = {};
      context.edi = 1;
      CEILING_CHECK(nsSpellBound(loop, &context) == NO_EXEC_DEFAULT);
      CEILING_CHECK(context.return_address == compare->GetAddress());
      CEILING_CHECK(nsSpellBound(compare, &context) == NO_EXEC_DEFAULT);
      CEILING_CHECK(context.return_address == 0x5BEA36);
      context.edi = activeSpellCount;
      nsSpellBound(loop, &context);
      CEILING_CHECK(context.return_address == 0x5BEA73);
   }

   void CheckDurationReset()
   {
      testName = "duration reset preserves effect queue";
      LoHook* reset = (LoHook*)_P->GetLastPatchAt(0x444260);
      CEILING_CHECK(reset != 0);
      unsigned char bytes[sizeof(army)] = {};
      army* stack = (army*)bytes;
      memset(&stack->SpellInfluenceQueue, 0x5A, sizeof(stack->SpellInfluenceQueue));
      unsigned char queue[sizeof(stack->SpellInfluenceQueue)];
      memcpy(queue, &stack->SpellInfluenceQueue, sizeof(queue));
      const int spells[] = {45, 95, 161, 162, 199};
      for (std::size_t i = 0; i < sizeof(spells) / sizeof(spells[0]); ++i)
      {
         nsDuration(stack, spells[i]) = 3;
         HookContext context = {};
         context.esi = (int)stack;
         context.eax = spells[i] - SPELL_WEAKNESS;
         nsSpellBound(reset, &context);
         CEILING_CHECK(nsDuration(stack, spells[i]) == 0);
         CEILING_CHECK(memcmp(queue, &stack->SpellInfluenceQueue, sizeof(queue)) == 0);
      }
      nsReleaseDummy(stack);
   }

   void CheckTemporaryDurations()
   {
      testName = "temporary-stack lifetime";
      const int stackCount = 96; // Exceed the former 64-entry rotating table.
      army* stacks = (army*)calloc(stackCount, sizeof(army));
      CEILING_CHECK(stacks != 0);
      int& heldDuration = nsDuration(&stacks[0], 199);
      heldDuration = 7;

      // A long-lived copy must survive repeated allocation/release of another.
      for (int i = 0; i < 128; ++i)
      {
         nsDuration(&stacks[1], 199) = i + 1;
         nsReleaseDummy(&stacks[1]);
      }
      CEILING_CHECK(nsDuration(&stacks[0], 199) == 7);
      CEILING_CHECK(heldDuration == 7);
      CEILING_CHECK(nsDuration(&stacks[1], 199) == 0);

      // Many simultaneous copies must keep independent values and references.
      for (int i = 1; i < stackCount; ++i)
      {
         nsDuration(&stacks[i], 162) = i;
         nsDuration(&stacks[i], 199) = i + 1;
      }
      CEILING_CHECK(heldDuration == 7 && &heldDuration == &nsDuration(&stacks[0], 199));
      for (int i = 1; i < stackCount; ++i)
         CEILING_CHECK(nsDuration(&stacks[i], 162) == i && nsDuration(&stacks[i], 199) == i + 1);
      for (int i = 0; i < stackCount; ++i)
         nsReleaseDummy(&stacks[i]);
      CEILING_CHECK(nsDuration(&stacks[0], 162) == 0 && nsDuration(&stacks[0], 199) == 0);
      nsReleaseDummy(&stacks[0]);
      free(stacks);
   }

   void CheckAiAndEffects()
   {
      testName = "AI and effect fixture";
      Game* savedGame = pGame;
      Game* game = (Game*)calloc(1, sizeof(Game));
      CEILING_CHECK(game != 0);
      pGame = game;
      CombatManager* savedCombat = pCombatManager;
      CombatManager* battle = (CombatManager*)calloc(1, sizeof(CombatManager));
      CEILING_CHECK(battle != 0);
      pCombatManager = battle;
      unsigned char opponent[sizeof(armyGroup)] = {};
      battle->hero[0] = &game->hero[0];
      battle->army[1] = (_Army_*)opponent;
      army* target = (army*)&battle->stack[0][0];
      target->group = 0; target->index = 0;
      target->numTroops = target->origNumTroops = 10;
      target->origHitPoints = target->sMonInfo.hitPoints = 20;
      target->poison_penalty = 1.0f;

      // Exercise the actual accessor at both storage boundaries, independently
      // of whether a data spell is defined at those IDs.
      testName = "disabled-flag boundaries";
      const int ids[] = {82, 95, 139, 140, 199};
      for (std::size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); ++i)
      {
         const int id = ids[i];
         const unsigned char saved = nsDisabledFlag(game, id);
         game->DisableSpell((SpellID)id, true);
         CEILING_CHECK(game->SpellDisabled((SpellID)id));
         game->DisableSpell((SpellID)id, false);
         if (id >= NS_DISABLED_SLOTS) ((unsigned char*)game)[4 + id] = 1;
         CEILING_CHECK(!game->SpellDisabled((SpellID)id));
         nsDisabledFlag(game, id) = saved;
      }

      const bool savedEvents = nsScriptEvents;
      nsScriptEvents = false;
      const bool fixture = getJsonInt("NewSpells.Test.CeilingFixture", 0) != 0;
      if (fixture)
      {
         testName = "required regression spells";
         CEILING_CHECK(isActiveExternalSpell(150) && isActiveExternalSpell(199));
         CEILING_CHECK(nsDataSpell(150) && nsDataSpell(150)->kind == NS_KIND_ENCHANTMENT);
         CEILING_CHECK(nsDataSpell(199) && nsDataSpell(199)->kind == NS_KIND_ENCHANTMENT);
      }
      int tested = 0;
      for (int i = 0; i < NS_DATA_SLOTS; ++i)
      {
         NsDataSpell& d = nsDataSpells[i];
         if (d.kind != NS_KIND_ENCHANTMENT || !isActiveExternalSpell(d.id))
            continue;
         char caseName[80];
         sprintf_s(caseName, "AI query and effect lifecycle, spell %d", d.id);
         testName = caseName;
         const unsigned char disabled = nsDisabledFlag(game, d.id);
         AiSpellStateV1 state = {}; state.size = sizeof(state);
         game->DisableSpell((SpellID)d.id, true);
         CEILING_CHECK(queryHeroSpellForAi(battle, 0, d.id, &state) == 1 && state.enabled == 0);
         game->DisableSpell((SpellID)d.id, false);
         if (d.id >= NS_DISABLED_SLOTS) ((unsigned char*)game)[4 + d.id] = 1;
         CEILING_CHECK(queryHeroSpellForAi(battle, 0, d.id, &state) == 1 && state.enabled == 1);
         nsDisabledFlag(game, d.id) = disabled;

         // Optional ERM callbacks belong to scenario scripts, absent at startup.
         char apply[64], remove[64];
         memcpy(apply, d.scriptOnApply, sizeof(apply));
         memcpy(remove, d.scriptOnRemove, sizeof(remove));
         d.scriptOnApply[0] = d.scriptOnRemove[0] = 0;
         target->SetSpellInfluence((SpellID)d.id, 3, 2, 0);
         CEILING_CHECK(nsDuration(target, d.id) == 3 && target->SpellInfluenceQueue.size == 1);
         target->CancelIndividualSpell(d.id);
         CEILING_CHECK(nsDuration(target, d.id) == 0 && target->SpellInfluenceQueue.size == 0);
         CEILING_CHECK(target->numSpellInfluences == 0 && target->sMonInfo.hitPoints == 20);
         if (d.id >= NS_DURATION_SLOTS)
         {
            target->SetSpellInfluence((SpellID)d.id, 2, 2, 0);
            nsNewRoundDurationsEx(target);
            CEILING_CHECK(nsDuration(target, d.id) == 1 && target->SpellInfluenceQueue.size == 1);
            nsNewRoundDurationsEx(target);
            CEILING_CHECK(nsDuration(target, d.id) == 0 && target->SpellInfluenceQueue.size == 0);
         }
         memcpy(d.scriptOnApply, apply, sizeof(apply));
         memcpy(d.scriptOnRemove, remove, sizeof(remove));
         ++tested;
      }
      testName = "data-spell coverage";
      if (fixture) CEILING_CHECK(tested == 2);
      char coverage[96];
      sprintf_s(coverage, "%d active enchantment(s) tested%s", tested,
         fixture ? " (required regression fixture)" : "");
      Era::WriteLog("Ceiling native probe", "Coverage", coverage);
      nsScriptEvents = savedEvents;
      pCombatManager = savedCombat;
      free(battle);
      pGame = savedGame;
      free(game);
   }

   void CheckRegressions()
   {
      CheckMageGuild();
      CheckDurationReset();
      CheckTemporaryDurations();
      CheckAiAndEffects();
      testName = "installed hooks and spell resources";
   }
}

void RunCeilingNativeProbe()
{
   char text[256];
   ceilingProbeTranslations("exe init hook");

   CeilingProbe::CheckRegressions();

   // Spell count and byte-bound compare sites.
   CEILING_CHECK(activeSpellCount >= DEFAULT_SPELLS_NUM && activeSpellCount <= WOG_SPELLS_MAX);
   CEILING_CHECK(activeSpellCount == getConfiguredSpellCount());
   CEILING_CHECK(getLiveSpellCount() == activeSpellCount);
   CEILING_CHECK(nsBoundSitesInstalled == sizeof(nsBoundSites) / sizeof(NsBoundSite));
   for (std::size_t i = 0; i < sizeof(nsBoundSites) / sizeof(NsBoundSite); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(nsBoundSites[i].address));
   CEILING_CHECK(nsFindBoundSite(0x402900) && nsFindBoundSite(0x5A852D));

   // Hero spellbook side table sites.
   CEILING_CHECK(heroSpellSitesInstalled == sizeof(heroSpellSites) / sizeof(heroSpellSites[0]));
   for (std::size_t i = 0; i < sizeof(heroSpellSites) / sizeof(heroSpellSites[0]); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(heroSpellSites[i].address));
   const int heroHooks[] = {0x433000, 0x433024, 0x5A027A, 0x4D8B6A, 0x4D89AF, 0x4D8F27, 0x486473, 0x48C940, 0x48CB50};
   for (std::size_t i = 0; i < sizeof(heroHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(heroHooks[i]));

   // Disabled flags above 139 and durations above 161.
   unsigned char game[256] = {};
   nsDisabledFlag(game, 10) = 1;
   nsDisabledFlag(game, NS_DISABLED_SLOTS + 10) = 1;
   CEILING_CHECK(game[4 + 10] == 1 && nsDisabledEx[10] == 1 && nsDisabledFlag(game, 139) == 0);
   nsDisabledFlag(game, NS_DISABLED_SLOTS + 10) = 0;
   const int disabledHooks[] = {0x4C16EA, 0x501294, 0x5BEA52};
   for (std::size_t i = 0; i < sizeof(disabledHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(disabledHooks[i]));

   unsigned char armyBytes[sizeof(army)] = {};
   army* fake = reinterpret_cast<army*>(armyBytes);
   CEILING_CHECK(!isRealArmy(fake));
   nsDuration(fake, 5) = 3;
   nsDuration(fake, NS_DURATION_SLOTS + 8) = 7;
   CEILING_CHECK(fake->spellInfluence[5] == 3 && nsDuration(fake, NS_DURATION_SLOTS + 8) == 7);
   CEILING_CHECK(nsDuration(fake, NS_DURATION_SLOTS + 9) == 0);
   nsReleaseDummy(fake);
   CEILING_CHECK(nsDurationLoopCount() == min(activeSpellCount, NS_DURATION_SLOTS));
   const int durationHooks[] = {0x43A395, 0x43A623, 0x444240, 0x4477A7, 0x437650, 0x43D120, 0x443F66, 0x5A190A, 0x5A1998};
   for (std::size_t i = 0; i < sizeof(durationHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(durationHooks[i]));

   // Chaining hooks and the scroll write inside the artifact updater.
   const int chainedHooks[] = {0x4489F0, 0x4422B0, 0x4D9840, 0x4D988D};
   for (std::size_t i = 0; i < sizeof(chainedHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(chainedHooks[i]));

   // Virtual LOD: the icon sheets come back padded.
   const int lodHooks[] = {0x4FB100, 0x4FACA0, 0x4FB1B0};
   for (std::size_t i = 0; i < sizeof(lodHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(lodHooks[i]));
   CEILING_CHECK(CeilingProbe::DefFrames("SpellInt.def") == SPELLS_MAX + 1);
   CEILING_CHECK(CeilingProbe::DefFrames("spells.def") == SPELLS_MAX);
   CEILING_CHECK(CeilingProbe::DefFrames("SpellScr.def") == SPELLS_MAX);
   CEILING_CHECK(CeilingProbe::DefFrames("SpellBon.def") == SPELLS_MAX);

   // Data spells: every loaded record is an active external slot with the engine tables set.
   int activeData = 0;
   for (int i = 0; i < NS_DATA_SLOTS; ++i)
   {
      const NsDataSpell& d = nsDataSpells[i];
      if (d.kind == NS_KIND_NONE)
         continue;
      CEILING_CHECK(nsDataProviderRegistered);
      CEILING_CHECK(d.id < activeSpellCount);
      CEILING_CHECK(isActiveExternalSpell(d.id));
      CEILING_CHECK(spellIndirectTableB[d.id - SPELL_QUICKSAND] == 17);
      CEILING_CHECK(spellIndirectTableF[d.id - SPELL_EARTHQUAKE] ==
         (d.kind == NS_KIND_DAMAGE || d.kind == NS_KIND_AREA_DAMAGE ? 2 : 9));
      CEILING_CHECK(o_Spell[d.id].name && *o_Spell[d.id].name && o_Spell[d.id].level >= 1);
      char* defName = 0;
      char* animName = 0;
      int animType = 0;
      if (getExternalAnimation(d.id, defName, animName, animType))
         CEILING_CHECK(CeilingProbe::DefFrames(defName) > 0);
      ++activeData;
   }
   CEILING_CHECK(activeData == nsDataSpellCount);

   sprintf_s(text, "PASS: %d checks, spell count %d, %d data spell(s)", CeilingProbe::checks, activeSpellCount, activeData);
   Era::WriteLog("Ceiling native probe", "Result", text);
}
