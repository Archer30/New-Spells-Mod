// Included only in the explicitly requested isolated native-probe build.
// Executes on the game thread after the core's spell table initialization.
int nativeChecks = 0;
int aiRandomCalls = 0;
int __stdcall ProbeRand(HiHook* hook)
{
   if (evaluationDepth) ++aiRandomCalls;
   return CALL_0(int, __cdecl, hook->GetDefaultFunc());
}
void NativeCheck(bool okay, int line)
{
   ++nativeChecks;
   if (!okay)
   {
      char message[80]; sprintf_s(message, "FAIL at line %d after %d checks", line, nativeChecks);
      Era::WriteLog("Blizzard native probe", "Result", message);
      RaiseException(0xE0420097, 0, 0, 0);
   }
}
#define NATIVE_CHECK(x) NativeCheck(!!(x), __LINE__)
void ProbeBody(CombatManager* bm)
{
   NewSpellsBattleContextV1 battle = {sizeof(battle), 1, bm, SpellId, NEWSPELLS_BATTLE_START, -1};
   Battle(&battle);
   for (int side = 0; side < 2; ++side)
      for (int slot = 0; slot < 21; ++slot)
      {
         army* stack = reinterpret_cast<army*>(&bm->stack[side][slot]);
         stack->group = side; stack->index = slot; stack->armyType = static_cast<TCreatureType>(0);
         stack->sMonInfo.hitPoints = 20; stack->sMonInfo.speed = 10; stack->slowPenalty = 1;
         stack->gridIndex = -1;
      }
   army* target = reinterpret_cast<army*>(&bm->stack[1][0]);
   target->numTroops = 10; target->gridIndex = 93;
   for (int first = 0; first < 4; ++first)
   {
      target->SetSpellInfluence(Id, 255, first, 0);
      NATIVE_CHECK(target->sMonInfo.speed == 10 - speedPenalty[first]);
      NATIVE_CHECK(target->spellInfluence[97] == 255 && target->numSpellInfluences == 1 && target->SpellInfluenceQueue.size == 1);
      for (int recast = 0; recast < 4; ++recast)
      {
         target->SetSpellInfluence(Id, 2, recast, 0);
         NATIVE_CHECK(target->sMonInfo.speed == 10 - speedPenalty[first] && target->spellInfluence[97] == 255);
         NATIVE_CHECK(target->numSpellInfluences == 1 && target->SpellInfluenceQueue.size == 1);
      }
      NewSpellsStatusContextV1 context = {sizeof(context), 1, bm, target, 0, SpellId, first, 254, NEWSPELLS_STATUS_ROUND};
      target->spellInfluence[97] = 254;
      NATIVE_CHECK(OnRound(&context) == NEWSPELLS_PROVIDER_COMMITTED && target->spellInfluence[97] == 255);
      NATIVE_CHECK(CureDispel(&context) == NEWSPELLS_PROVIDER_DENIED);
      // The death-only call adapter must retain the native influence/queue.
      target->numTroops = 0; target->bAllUnitsKilled = 1;
      DeathRemoval(0, target, SpellId);
      NATIVE_CHECK(target->spellInfluence[97] == 255 && target->SpellInfluenceQueue.size == 1);
      target->numTroops = 10; target->bAllUnitsKilled = 0;
      target->SetSpellInfluence(Id, 255, 3, 0);
      NATIVE_CHECK(target->sMonInfo.speed == 10 - speedPenalty[first]);
      target->CancelIndividualSpell(97);
      NATIVE_CHECK(target->sMonInfo.speed == 10 && !target->spellInfluence[97] && !target->numSpellInfluences && !target->SpellInfluenceQueue.size);
   }
   for (int speed = 1; speed <= 5; ++speed)
   {
      target->sMonInfo.speed = speed;
      target->SetSpellInfluence(Id, 255, 3, 0);
      NATIVE_CHECK(target->sMonInfo.speed == max(1, speed - 4));
      target->CancelIndividualSpell(97);
      NATIVE_CHECK(target->sMonInfo.speed == speed && !target->SpellInfluenceQueue.size);
   }
   target->sMonInfo.speed = 10;
   target->SetSpellInfluence(Id, 255, 0, 0);
   target->SetSpellInfluence(SPELL_HASTE, 5, 3, 0);
   NATIVE_CHECK(target->sMonInfo.speed == 13);
   target->CancelIndividualSpell(SPELL_HASTE);
   NATIVE_CHECK(target->sMonInfo.speed == 8);
   target->SetSpellInfluence(SPELL_SLOW, 5, 3, 0);
   NATIVE_CHECK(target->sMonInfo.speed == 8 && Speed(target) == 4);
   target->CancelIndividualSpell(SPELL_SLOW);
   NATIVE_CHECK(target->sMonInfo.speed == 8 && target->SpellInfluenceQueue.size == 1);
   battle.event = NEWSPELLS_BATTLE_END; Battle(&battle);
   NATIVE_CHECK(target->sMonInfo.speed == 10 && !target->SpellInfluenceQueue.size);
   battle.event = NEWSPELLS_BATTLE_START; Battle(&battle);
   target->SetSpellInfluence(Id, 255, 3, 0);
   NATIVE_CHECK(target->sMonInfo.speed == 6);
   target->CancelIndividualSpell(97);
   target->sMonInfo.speed = 1;
   target->SetSpellInfluence(SPELL_HASTE, 5, 3, 0);
   target->SetSpellInfluence(Id, 255, 3, 0);
   target->CancelIndividualSpell(SPELL_HASTE);
   NATIVE_CHECK(Speed(target) == 1 && status[1][0].penalty == 4);
   target->CancelIndividualSpell(97);
   NATIVE_CHECK(target->sMonInfo.speed == 1);
   target->sMonInfo.speed = 10;
   // ERM operates on status only, never on the stack's hit points.
   NewSpellsCombatContextV1 erm = {sizeof(erm), 1, bm, 0, 0, target, SpellId, 0, 93, 2, 3, NEWSPELLS_SOURCE_ERM};
   target->spellInfluence[97] = 3;
   NATIVE_CHECK(Erm(&erm) == NEWSPELLS_PROVIDER_COMMITTED);
   NATIVE_CHECK(target->numTroops == 10 && target->residualDamage == 0 && target->sMonInfo.speed == 6 && target->SpellInfluenceQueue.size == 1);
   target->CancelIndividualSpell(97);
   // Compare the provider collector against native radius-2 geometry for
   // every legal center and every legal single-hex target position.
   for (int center = 0; center < 187; ++center)
   {
      if (!BlizzardMath::Hex(center)) continue;
      exe_vector<int> cells = {};
      CALL_5(void, __thiscall, 0x5A4480, bm, center, 2, 1, &cells);
      bool expected[187] = {};
      for (size_t i = 0; i < cells.size(); ++i) if (BlizzardMath::Hex(cells.first[i])) expected[cells.first[i]] = true;
      o_Delete(cells.first);
      for (int hex = 0; hex < 187; ++hex)
      {
         if (!BlizzardMath::Hex(hex)) continue;
         target->gridIndex = hex;
         army* targets[42] = {};
         NATIVE_CHECK(Targets(bm, center, targets) == (expected[hex] ? 1 : 0));
      }
      target->gridIndex = 93; target->sMonInfo.attributes |= CF_DOUBLE_WIDE;
      const int rear = CALL_1(int, __thiscall, 0x4463C0, target);
      army* doubleTargets[42] = {};
      NATIVE_CHECK(Targets(bm, center, doubleTargets) == (expected[93] || expected[rear] ? 1 : 0));
      target->sMonInfo.attributes &= ~CF_DOUBLE_WIDE;
   }
   target->gridIndex = 93; target->sMonInfo.attributes |= CF_DOUBLE_WIDE;
   army* results[42] = {};
   NATIVE_CHECK(Targets(bm, 93, results) == 1 && results[0] == target);
   target->sMonInfo.attributes &= ~CF_DOUBLE_WIDE;
   army* ally = reinterpret_cast<army*>(&bm->stack[0][20]);
   ally->numTroops = 1; ally->gridIndex = 94;
   NATIVE_CHECK(Targets(bm, 93, results) == 2); // friendly fire and slot 20
   // Actual native damage entry under the same scope used by area execution.
   NewSpellsCombatContextV1 cast = {sizeof(cast), 1, bm, 0, 0, target, SpellId, 0, 93, 0, 1, NEWSPELLS_SOURCE_HERO};
   casting = &cast; chanceTarget = target;
   CALL_2(int, __thiscall, 0x443DB0, target, 5);
   casting = 0; chanceTarget = 0;
   NATIVE_CHECK(target->residualDamage == 5 && target->sMonInfo.speed == 8 && target->SpellInfluenceQueue.size == 1);
   target->CancelIndividualSpell(97);
   target->residualDamage = 0;
   // Native AI valuation on deterministic synthetic stacks, with the real
   // installed immunity/damage/resistance chains and a random-call observer.
   ally->gridIndex = 18; ally->numTroops = 10;
   for (int i = 0; i < 2; ++i)
   {
      army* stack = i ? target : ally;
      stack->sMonInfo.baseFightValue = stack->sMonInfo.AI_value = 100;
      stack->sMonInfo.attackSkill = stack->sMonInfo.defenseSkill = 5;
      stack->sMonInfo.damageLowBound = stack->sMonInfo.damageHighBound = 3;
      stack->AI_target = i ? ally : target; stack->AI_target_distance = 14;
   }
   type_AI_spellcaster planner = {}, mirror = {};
   planner.our_group = 0; planner.enemy_group = 1; planner.enemy_caster = &mirror;
   mirror.our_group = 1; mirror.enemy_group = 0; mirror.enemy_caster = &planner;
   planner.estimate.rounds_left = mirror.estimate.rounds_left = 10;
   planner.estimate.friendly_combat_value = mirror.estimate.friendly_combat_value = 10000;
   planner.estimate.enemy_combat_value = mirror.estimate.enemy_combat_value = 10000;
   planner.estimate.lowest_attack = planner.estimate.lowest_defense = 5;
   mirror.estimate.lowest_attack = mirror.estimate.lowest_defense = 5;
   NewSpellsAiContextV1 ai = {sizeof(ai), 1, &planner, bm, 0, 0, SpellId, 0, -1, 3, 1, 255, 0, 0};
   Patch* randObserver = owner->WriteHiHook(0x61842C, SPLICE_, EXTENDED_, CDECL_, ProbeRand);
   unsigned char* before = reinterpret_cast<unsigned char*>(o_New(sizeof(CombatManager)));
   std::memcpy(before, bm, sizeof(CombatManager));
   NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && ai.castNow == 1 && ai.score > 0);
   const int selected = ai.targetHex, score = ai.score;
   for (int repeat = 0; repeat < 5; ++repeat)
      NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && ai.targetHex == selected && ai.score == score);
   NATIVE_CHECK(aiRandomCalls == 0 && std::memcmp(before, bm, sizeof(CombatManager)) == 0);
   target->SetSpellInfluence(Id, 255, 3, 0);
   NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && ai.score < score && ai.score > 0);
   target->CancelIndividualSpell(97);
   target->spellInfluence[SPELL_ANTI_MAGIC] = 5; target->antiMagicSpellLevel = 6;
   NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && !ai.castNow && ai.score == 0);
   target->spellInfluence[SPELL_ANTI_MAGIC] = 0; target->antiMagicSpellLevel = 0;
   army* aura = ally;
   target->aura_sources.first = &aura; target->aura_sources.last = &aura + 1;
   {
      EvaluationScope resistanceScope;
      const float chance = bm->SpellCastWorkChance(SpellId, 0, target, false, true, false);
      NATIVE_CHECK(chance > 0.79f && chance < 0.81f);
   }
   NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && ai.score < score && ai.score > 0);
   target->aura_sources.first = target->aura_sources.last = 0;
   target->numTroops = 1;
   NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && ai.score > 0);
   const int lethalScore = ai.score;
   target->SetSpellInfluence(Id, 255, 3, 0);
   NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && ai.score == lethalScore);
   target->CancelIndividualSpell(97); target->numTroops = 10;
   // An equally valuable friendly stack on the same hex vetoes every hit.
   ally->gridIndex = target->gridIndex;
   NATIVE_CHECK(Evaluate(&ai) == NEWSPELLS_PROVIDER_COMMITTED && !ai.castNow);
   NATIVE_CHECK(aiRandomCalls == 0);
   randObserver->Undo(); randObserver->Destroy(); o_Delete(before);
   target->SetSpellInfluence(Id, 255, 0, 0);
   CALL_1(void, __thiscall, 0x43D2E0, target);
   target->group = 1; target->index = 0; target->armyType = static_cast<TCreatureType>(0);
   target->numTroops = 10; target->bAllUnitsKilled = 0;
   target->sMonInfo.speed = 10; target->sMonInfo.hitPoints = 20;
   target->sMonInfo.attributes = 0;
   erm.mastery = 3; target->spellInfluence[97] = 3;
   NATIVE_CHECK(Erm(&erm) == NEWSPELLS_PROVIDER_COMMITTED && target->sMonInfo.speed == 6);
   NATIVE_CHECK(target->SpellInfluenceQueue.size == 1 && status[1][0].penalty == 4);
   battle.event = NEWSPELLS_BATTLE_END; Battle(&battle);
}
int RunNativeProbe()
{
   CombatManager* saved = pCombatManager;
   CombatManager* temporary = reinterpret_cast<CombatManager*>(o_New(sizeof(CombatManager)));
   if (!temporary) return 0;
   std::memset(temporary, 0, sizeof(CombatManager));
   pCombatManager = temporary;
   int result = 0;
   __try
   {
      ProbeBody(temporary);
      result = 1;
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      char message[100]; sprintf_s(message, "Native exception %08X after %d checks", GetExceptionCode(), nativeChecks);
      Era::WriteLog("Blizzard native probe", "Result", message);
   }
   pCombatManager = saved;
   // The process is a disposable probe. Leave its temporary native queue
   // allocation to process teardown; never run an army destructor on a mock.
   char message[80]; sprintf_s(message, "%s: %d checks", result ? "PASS" : "FAIL", nativeChecks);
   Era::WriteLog("Blizzard native probe", "Result", message);
   return result;
}
#undef NATIVE_CHECK
