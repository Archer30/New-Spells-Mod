#include "stdafx.h"
#include "../NewSpells/NewSpells.h"
#include "Blizzard.h"
#include "BlizzardMath.h"
#include <cstring>
#include <cmath>

namespace Blizzard
{
namespace
{
   const SpellID Id = static_cast<SpellID>(SpellId);
   const int Permanent = 255;
   struct Status { bool applied; int penalty; int creature; };
   Status status[2][21] = {};
   Patcher* patcher = 0;
   PatcherInstance* owner = 0;
   HiHook* coreChance = 0;
   HiHook* chanceHook = 0;
   Patch* hooks[12] = {};
   unsigned hookCount = 0;
   bool ready = false;
   int areaSpell = -1;
   int evaluationDepth = 0;
   int damageDepth = 0;
   const NewSpellsCombatContextV1* casting = 0;
   army* chanceTarget = 0;
   int speedPenalty[4] = {2, 2, 4, 4};

   struct Scope
   {
      int previous;
      explicit Scope(int spell) : previous(areaSpell) { areaSpell = spell; }
      ~Scope() { areaSpell = previous; }
   };
   struct EvaluationScope
   {
      Scope spell;
      EvaluationScope() : spell(SpellId) { ++evaluationDepth; }
      ~EvaluationScope() { --evaluationDepth; }
   };

   bool Slot(const army* stack, unsigned& slot)
   {
      if (!stack || !pCombatManager) return false;
      const uintptr_t first = reinterpret_cast<uintptr_t>(&pCombatManager->stack[0][0]);
      const uintptr_t ptr = reinterpret_cast<uintptr_t>(stack);
      if (ptr < first || ptr - first >= sizeof(pCombatManager->stack) ||
          (ptr - first) % sizeof(army)) return false;
      slot = static_cast<unsigned>((ptr - first) / sizeof(army));
      return true;
   }
   bool Live(const army* stack)
   {
      unsigned slot = 0;
      if (!Slot(stack, slot)) return false;
      return stack->group == static_cast<int>(slot / 21) && stack->index == static_cast<int>(slot % 21);
   }
   void __stdcall ResetStack(HiHook* hook, army* stack)
   {
      unsigned slot = 0;
      // InitClean is used for fresh/summoned stacks, never resurrection.
      // Address identity works even before group/index are initialized.
      if (Slot(stack, slot)) status[slot / 21][slot % 21] = Status();
      CALL_1(void, __thiscall, hook->GetDefaultFunc(), stack);
   }
   bool Alive(const army* stack)
   {
      return Live(stack) && stack->numTroops > 0 && !stack->bAllUnitsKilled && stack->sMonInfo.hitPoints > 0;
   }
   int Speed(const army* stack)
   {
      return CALL_1(int, __thiscall, 0x4489F0, stack);
   }
   bool Readable(const void* pointer, size_t size)
   {
      uintptr_t cursor = reinterpret_cast<uintptr_t>(pointer), end = cursor + size;
      if (!cursor || end < cursor) return false;
      while (cursor < end)
      {
         MEMORY_BASIC_INFORMATION memory = {};
         if (!VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) ||
             memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
         uintptr_t next = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
         if (next <= cursor) return false;
         cursor = next;
      }
      return true;
   }
   int __stdcall EffectiveSpeed(HiHook* hook, army* stack)
   {
      const int speed = CALL_1(int, __thiscall, hook->GetDefaultFunc(), stack);
      // Haste/Prayer can expire after the first penalty was recorded. Keep
      // the stored subtraction intact while retaining the speed floor.
      return ready && Live(stack) && stack->spellInfluence[SpellId] &&
         !(stack->sMonInfo.attributes & CF_SIEGE_WEAPON) ? max(1, speed) : speed;
   }
   int Health(const army* stack)
   {
      return BlizzardMath::Positive(static_cast<int64_t>(stack->numTroops) * stack->sMonInfo.hitPoints - stack->residualDamage);
   }
   bool Executable(uintptr_t address)
   {
      MEMORY_BASIC_INFORMATION memory = {};
      if (!address || !VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)) ||
          memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
      return (memory.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
   }
   bool ChanceChain()
   {
      return coreChance && chanceHook && coreChance->IsApplied() && chanceHook->IsApplied() &&
         coreChance->GetAppliedAfter() == chanceHook && chanceHook->GetAppliedBefore() == coreChance &&
         chanceHook->GetDefaultFunc() != coreChance->GetDefaultFunc() && Executable(coreChance->GetDefaultFunc());
   }
   bool IsBlizzardArea(CombatManager* bm)
   {
      return ready && bm == pCombatManager && (areaSpell == SpellId ||
         (areaSpell < 0 && bm->Field<int>(0x40) == SpellId));
   }

   // The integer-hex overload has no ArmyEffected/UI writes. Both execution
   // and AI use its exact native radius geometry, including battlefield edges.
   int Targets(CombatManager* bm, int hex, army** result)
   {
      exe_vector<int> cells = {};
      CALL_5(void, __thiscall, 0x5A4480, bm, hex, 2, 1, &cells);
      bool covered[187] = {};
      for (size_t i = 0; i < cells.size() && i < 187; ++i)
         if (BlizzardMath::Hex(cells.first[i])) covered[cells.first[i]] = true;
      o_Delete(cells.first);
      int count = 0;
      for (int side = 0; side < 2; ++side)
         for (int slot = 0; slot < 21; ++slot)
         {
            army* stack = reinterpret_cast<army*>(&bm->stack[side][slot]);
            if (!Alive(stack) || (stack->sMonInfo.attributes & (CF_IMMOBILIZED | CF_SACRIFICED))) continue;
            bool hit = BlizzardMath::Hex(stack->gridIndex) && covered[stack->gridIndex];
            if (!hit && (stack->sMonInfo.attributes & CF_DOUBLE_WIDE))
            {
               const int second = CALL_1(int, __thiscall, 0x4463C0, stack);
               hit = BlizzardMath::Hex(second) && covered[second];
            }
            if (hit) result[count++] = stack;
         }
      return count;
   }

   void __stdcall MarkHexes(HiHook* hook, CombatManager* bm, int hex, int radius, int center, exe_vector<int>* result)
   {
      if (IsBlizzardArea(bm)) { radius = 2; center = 1; }
      CALL_5(void, __thiscall, hook->GetDefaultFunc(), bm, hex, radius, center, result);
   }
   void __stdcall MarkArmies(HiHook* hook, CombatManager* bm, int hex, int radius, int center, exe_vector<army*>* result)
   {
      if (!IsBlizzardArea(bm))
      {
         CALL_5(void, __thiscall, hook->GetDefaultFunc(), bm, hex, radius, center, result);
         return;
      }
      army* targets[42] = {};
      const int count = Targets(bm, hex, targets);
      army** buffer = count ? reinterpret_cast<army**>(o_New(count * sizeof(army*))) : 0;
      if (count && !buffer) return;
      if (count) std::memcpy(buffer, targets, count * sizeof(army*));
      o_Delete(result->first);
      result->first = buffer;
      result->last = result->end = buffer ? buffer + count : 0;
      // Native drawing uses two arrays of 20 flags. The returned target list
      // itself is deduplicated over all 42 slots and never aliases slot 20.
      std::memset(reinterpret_cast<char*>(bm) + 0x547C, 0, 40);
      for (int i = 0; i < count; ++i)
         if (targets[i]->index < 20)
            massSpellTarget[targets[i]->group][targets[i]->index] = true;
   }
   void __stdcall MarkSpell(HiHook* hook, CombatManager* bm, int spell, int hex, int mastery, exe_vector<army*>* result)
   {
      Scope scope(spell);
      CALL_5(void, __thiscall, hook->GetDefaultFunc(), bm, spell, hex, mastery, result);
   }
   float __stdcall WorkChance(HiHook* hook, CombatManager* bm, int spell, int side, army* target,
      bool redirected, bool first, bool creature)
   {
      if (spell != SpellId || (!casting && !evaluationDepth))
         return CALL_7(float, __thiscall, hook->GetDefaultFunc(), bm, spell, side, target, redirected, first, creature);
      if (!ready || !ChanceChain() || !Alive(target) || side < 0 || side > 1) return 0.0f;
      if (casting) chanceTarget = target;
      // Bypass only the core provider shortcut. Earlier Amethyst/native hooks
      // and later Emerald/other hooks retain their normal chain positions.
      const float chance = CALL_7(float, __thiscall, coreChance->GetDefaultFunc(),
         bm, spell, side, target, redirected, first, creature);
      return chance >= 0.0f && chance <= 1.0f ? chance : 0.0f;
   }
   int __stdcall Damage(HiHook* hook, army* stack, int damage)
   {
      const bool apply = ready && casting && !damageDepth && stack == chanceTarget && Alive(stack);
      const NewSpellsCombatContextV1* context = casting;
      ++damageDepth;
      const int result = CALL_2(int, __thiscall, hook->GetDefaultFunc(), stack, damage);
      --damageDepth;
      if (apply && Alive(stack) && context == casting)
      {
         chanceTarget = 0;
         stack->SetSpellInfluence(Id, Permanent, context->mastery, static_cast<const hero*>(context->casterHero));
      }
      return result;
   }
   void __stdcall DeathRemoval(HiHook* hook, army* stack, int spell)
   {
      if (ready && spell == SpellId && Live(stack) && status[stack->group][stack->index].applied) return;
      CALL_2(void, __thiscall, hook->GetDefaultFunc(), stack, spell);
   }
   bool ValidStatus(const NewSpellsStatusContextV1* context)
   {
      return context && context->size >= sizeof(*context) && context->abiVersion == 1 &&
         context->spellId == SpellId && context->combatManager == pCombatManager && Live(static_cast<army*>(context->stack));
   }
   int32_t __stdcall Apply(NewSpellsStatusContextV1* context)
   {
      if (!ready) return NEWSPELLS_PROVIDER_UNSUPPORTED;
      if (!ValidStatus(context) || context->mastery < 0 || context->mastery > 3) return NEWSPELLS_PROVIDER_DENIED;
      army* stack = static_cast<army*>(context->stack);
      Status& state = status[stack->group][stack->index];
      if (!state.applied)
      {
         if (!Alive(stack) || (stack->sMonInfo.attributes & CF_SIEGE_WEAPON) || stack->sMonInfo.speed <= 0)
            return NEWSPELLS_PROVIDER_DENIED;
         state.penalty = BlizzardMath::Penalty(stack->sMonInfo.speed, speedPenalty[context->mastery]);
         state.creature = stack->armyType;
         state.applied = true;
         stack->sMonInfo.speed -= state.penalty;
      }
      stack->spellInfluence[SpellId] = Permanent;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }
   int32_t __stdcall OnRound(NewSpellsStatusContextV1* context)
   {
      if (!ValidStatus(context)) return NEWSPELLS_PROVIDER_DENIED;
      army* stack = static_cast<army*>(context->stack);
      if (!status[stack->group][stack->index].applied) return NEWSPELLS_PROVIDER_DENIED;
      stack->spellInfluence[SpellId] = Permanent;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }
   int32_t __stdcall Remove(NewSpellsStatusContextV1* context)
   {
      if (!ValidStatus(context)) return NEWSPELLS_PROVIDER_DENIED;
      army* stack = static_cast<army*>(context->stack);
      Status& state = status[stack->group][stack->index];
      if (state.applied && state.creature == stack->armyType)
         stack->sMonInfo.speed = BlizzardMath::Clamp(static_cast<int64_t>(stack->sMonInfo.speed) + state.penalty);
      state = Status();
      return NEWSPELLS_PROVIDER_COMMITTED;
   }
   int32_t __stdcall CureDispel(NewSpellsStatusContextV1* context)
   {
      return ValidStatus(context) ? NEWSPELLS_PROVIDER_DENIED : NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   int32_t __stdcall Battle(NewSpellsBattleContextV1* context)
   {
      if (!context || context->size < sizeof(*context) || context->abiVersion != 1 || context->spellId != SpellId)
         return NEWSPELLS_PROVIDER_DENIED;
      if (context->event == NEWSPELLS_BATTLE_END && context->combatManager == pCombatManager)
         for (int side = 0; side < 2; ++side)
            for (int slot = 0; slot < 21; ++slot)
            {
               army* stack = reinterpret_cast<army*>(&pCombatManager->stack[side][slot]);
               if (Live(stack) && stack->spellInfluence[SpellId]) stack->CancelIndividualSpell(SpellId);
            }
      std::memset(status, 0, sizeof(status));
      casting = 0; chanceTarget = 0;
      return ready ? NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   int32_t __stdcall Validate(NewSpellsCombatContextV1* context)
   {
      if (!ready || !ChanceChain()) return NEWSPELLS_PROVIDER_UNSUPPORTED;
      if (!context || context->size < sizeof(*context) || context->abiVersion != 1 || context->spellId != SpellId ||
          !pCombatManager || context->combatManager != pCombatManager || context->casterSide < 0 || context->casterSide > 1 ||
          context->mastery < 0 || context->mastery > 3 || context->spellPower < 0 || !BlizzardMath::Hex(context->targetHex))
         return NEWSPELLS_PROVIDER_DENIED;
      // A cast's base damage must be representable before entering native code.
      const int64_t damage = static_cast<int64_t>(context->spellPower) * o_Spell[SpellId].eff_power + o_Spell[SpellId].effect[context->mastery];
      return damage >= 0 && damage <= INT_MAX ? NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_DENIED;
   }
   int32_t CastImpl(NewSpellsCombatContextV1* context)
   {
      const int valid = Validate(context);
      if (valid != NEWSPELLS_PROVIDER_COMMITTED || casting) return valid == NEWSPELLS_PROVIDER_COMMITTED ? NEWSPELLS_PROVIDER_DENIED : valid;
      // A summoned replacement may reuse a former corpse's slot after the
      // engine has initialized a fresh influence table.
      for (int side = 0; side < 2; ++side)
         for (int slot = 0; slot < 21; ++slot)
            if (!reinterpret_cast<army*>(&pCombatManager->stack[side][slot])->spellInfluence[SpellId])
               status[side][slot] = Status();
      Scope scope(SpellId);
      casting = context;
      chanceTarget = 0;
      pCombatManager->AreaEffect(context->targetHex, SpellId, static_cast<TSkillMastery>(context->mastery), context->spellPower);
      chanceTarget = 0;
      casting = 0;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }
   int32_t __stdcall Erm(NewSpellsCombatContextV1* context)
   {
      if (!ready) return NEWSPELLS_PROVIDER_UNSUPPORTED;
      if (!context || context->size < sizeof(*context) || context->abiVersion != 1 || context->spellId != SpellId ||
          context->source != NEWSPELLS_SOURCE_ERM || context->combatManager != pCombatManager ||
          !Live(static_cast<army*>(context->targetStack)) || context->mastery < 0 || context->mastery > 3)
         return NEWSPELLS_PROVIDER_DENIED;
      army* stack = static_cast<army*>(context->targetStack);
      if (context->spellPower == 0) return NEWSPELLS_PROVIDER_COMMITTED; // core calls the native remover
      Status& state = status[stack->group][stack->index];
      if (!state.applied)
      {
         // BM:C has provisionally written the duration, but not the native
         // queue/count. Restore an absent influence before native insertion.
         stack->spellInfluence[SpellId] = 0;
         stack->SetSpellInfluence(Id, Permanent, context->mastery, 0);
      }
      else stack->spellInfluence[SpellId] = Permanent;
      return state.applied ? NEWSPELLS_PROVIDER_COMMITTED : NEWSPELLS_PROVIDER_DENIED;
   }

   int SlowValue(const type_AI_spellcaster* planner, army* target, int newSpeed, double survivingFraction)
   {
      const int rounds = planner->estimate.rounds_left;
      const int oldSpeed = Speed(target);
      if (rounds <= 0 || oldSpeed < 1 || planner->win_likely || !Alive(target->AI_target) || newSpeed >= oldSpeed || newSpeed < 1) return 0;
      const int arrival = target->get_AI_target_time(oldSpeed);
      const int duration = rounds - ((target->sMonInfo.attributes & CF_DONE) ? 1 : 0);
      if (arrival < 0 || arrival > rounds || duration <= 0) return 0;
      int64_t value = 0;
      if (arrival == 1)
         for (int slot = 0; slot < 21; ++slot)
         {
            army* ally = reinterpret_cast<army*>(&pCombatManager->stack[planner->our_group][slot]);
            if (!Alive(ally) || ally->AI_target != target || ally->spellInfluence[SPELL_BLIND] ||
                ally->spellInfluence[SPELL_STONE_GAZE] || ally->spellInfluence[SPELL_PARALYZE] ||
                (ally->sMonInfo.attributes & (CF_IMMOBILIZED | CF_SIEGE_WEAPON))) continue;
            const int speed = Speed(ally);
            if (speed > oldSpeed || speed <= newSpeed) continue;
            const int64_t exchange = static_cast<int64_t>(planner->estimate.get_exchange_effect(ally, target)) +
               planner->estimate.get_exchange_effect(target, ally);
            if (exchange > value) value = exchange;
         }
      if (!target->can_shoot())
      {
         const int delayed = target->get_AI_target_time(newSpeed);
         const int delay = min(duration, BlizzardMath::Positive(static_cast<int64_t>(delayed) - arrival));
         const int benefit = min(delay, BlizzardMath::Positive(static_cast<int64_t>(rounds) - arrival + 1));
         if (benefit > 0)
            value += static_cast<int64_t>(max(0, target->get_total_combat_value(planner->estimate.lowest_attack,
               planner->estimate.lowest_defense))) * benefit / rounds;
      }
      const double adjusted = static_cast<double>(value) * survivingFraction;
      return adjusted >= INT_MAX ? INT_MAX : adjusted <= 0 ? 0 : static_cast<int>(adjusted);
   }
   int32_t EvaluateImpl(NewSpellsAiContextV1* context)
   {
      if (!context || context->size < sizeof(*context)) return NEWSPELLS_PROVIDER_DENIED;
      context->score = 0; context->castNow = 0; context->targetHex = -1;
      if (!ready || !ChanceChain()) return NEWSPELLS_PROVIDER_UNSUPPORTED;
      if (context->abiVersion != 1 || context->spellId != SpellId || !pCombatManager ||
          context->combatManager != pCombatManager || context->casterSide < 0 || context->casterSide > 1 ||
          context->mastery < 0 || context->mastery > 3 || context->spellPower < 0 ||
          !Readable(context->planner, sizeof(type_AI_spellcaster)) ||
          (context->casterHero && !Readable(context->casterHero, sizeof(hero))))
         return NEWSPELLS_PROVIDER_DENIED;
      const type_AI_spellcaster* planner = static_cast<type_AI_spellcaster*>(context->planner);
      if (planner->our_group != context->casterSide || planner->enemy_group != 1 - context->casterSide ||
          planner->estimate.rounds_left <= 0 || planner->current_hero != context->casterHero)
         return NEWSPELLS_PROVIDER_DENIED;
      EvaluationScope scope;
      const int base = BlizzardMath::BaseDamage(context->spellPower, o_Spell[SpellId].eff_power, o_Spell[SpellId].effect[context->mastery]);
      int values[2][21] = {};
      for (int side = 0; side < 2; ++side)
         for (int slot = 0; slot < 21; ++slot)
         {
            army* target = reinterpret_cast<army*>(&pCombatManager->stack[side][slot]);
            if (!Alive(target)) continue;
            const hero* controller = CALL_1(const hero*, __thiscall, 0x4423B0, target);
            const int damageValue = CALL_5(int, __thiscall, 0x436A80, planner, Id, base, controller, target);
            int slow = 0;
            if (!target->spellInfluence[SpellId] &&
                !(target->sMonInfo.attributes & CF_SIEGE_WEAPON))
            {
               const float chance = pCombatManager->SpellCastWorkChance(SpellId, context->casterSide, target, false, true, planner->is_creature_spell);
               const int damage = max(0, CALL_7(int, __thiscall, 0x5A7BF0, pCombatManager, base, SpellId,
                  context->casterHero, controller, target, false));
               const int health = Health(target);
               if (chance > 0 && health > damage)
               {
                  const int penalty = BlizzardMath::Penalty(target->sMonInfo.speed, speedPenalty[context->mastery]);
                  int speed = target->sMonInfo.speed - penalty;
                  if (target->spellInfluence[SPELL_SLOW] || target->spellInfluence[SPELL_DISEASE] || target->spellInfluence[SPELL_FEAR])
                  {
                     const double adjusted = static_cast<double>(speed) * target->slowPenalty;
                     if (!(adjusted >= 0 && adjusted <= INT_MAX)) continue;
                     speed = max(1, static_cast<int>(adjusted));
                  }
                  const type_AI_spellcaster* perspective = side == context->casterSide ? planner->enemy_caster : planner;
                  if (Readable(perspective, sizeof(*perspective)) && perspective->our_group == 1 - side)
                     slow = SlowValue(perspective, target, speed, chance * static_cast<double>(health - damage) / health);
               }
            }
            values[side][slot] = BlizzardMath::Clamp(static_cast<int64_t>(damageValue) + slow);
         }
      for (int hex = 0; hex < 187; ++hex)
      {
         if (!BlizzardMath::Hex(hex)) continue;
         army* targets[42] = {};
         const int count = Targets(pCombatManager, hex, targets);
         int64_t enemy = 0, friendly = 0;
         for (int i = 0; i < count; ++i)
         {
            const int value = values[targets[i]->group][targets[i]->index];
            if (targets[i]->group == context->casterSide) friendly += value;
            else enemy += value;
         }
         const int score = BlizzardMath::AreaValue(enemy, friendly, planner->estimate.enemy_combat_value, planner->estimate.friendly_combat_value);
         if (score > context->score) { context->score = score; context->targetHex = hex; context->castNow = 1; }
      }
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   int32_t __stdcall Cast(NewSpellsCombatContextV1* context)
   {
      const int savedArea = areaSpell;
      const int savedDepth = damageDepth;
      const NewSpellsCombatContextV1* savedCast = casting;
      army* savedTarget = chanceTarget;
      __try { return CastImpl(context); }
      __finally
      {
         areaSpell = savedArea; damageDepth = savedDepth;
         casting = savedCast; chanceTarget = savedTarget;
      }
   }
   int32_t __stdcall Evaluate(NewSpellsAiContextV1* context)
   {
      const int savedArea = areaSpell, savedDepth = evaluationDepth;
      __try { return EvaluateImpl(context); }
      __finally { areaSpell = savedArea; evaluationDepth = savedDepth; }
   }

   template<size_t N> bool Chain(uintptr_t address, const unsigned char (&signature)[N])
   {
      Patch* first = patcher->GetFirstPatchAt(address);
      if (!first) return std::memcmp(reinterpret_cast<void*>(address), signature, N) == 0;
      size_t overwritten = 0;
      for (Patch* patch = first; patch; patch = patch->GetAppliedAfter())
      {
         if (!patch->IsApplied() || patch->GetAddress() != address || patch->GetType() != HIHOOK_ ||
             patch->GetSize() < 5 || patch->GetSize() > 10) return false;
         if (patch->GetSize() > overwritten) overwritten = patch->GetSize();
         if (!Executable(static_cast<HiHook*>(patch)->GetDefaultFunc())) return false;
      }
      const uintptr_t original = static_cast<HiHook*>(first)->GetOriginalFunc();
      // ERA can wrap the original trampoline in its guarded hook dispatcher.
      // Validate the untouched native instruction tail after the registered
      // splice span, and the complete applied HiHook chain. Do not jump into
      // or bypass that dispatcher to recover an assumed bare prologue.
      return Executable(original) && overwritten < N &&
         std::memcmp(reinterpret_cast<void*>(address + overwritten), signature + overwritten, N - overwritten) == 0;
   }
   bool Keep(Patch* patch)
   {
      if (!patch || hookCount >= sizeof(hooks) / sizeof(hooks[0])) return false;
      hooks[hookCount++] = patch;
      return patch->IsApplied();
   }
}

bool Initialize(Patcher* source)
{
   if (ready) return true;
   patcher = source;
   if (!patcher) return false;
   const unsigned char chanceEntry[] = {0x55,0x8B,0xEC,0x83,0xEC,0x0C,0x8B,0x45,0x0C,0x53,0x56,0x8B,0xF1,0x57,0x8B,0x7D,0x10,0x8B};
   const unsigned char damageEntry[] = {0x55,0x8B,0xEC,0x53,0x56,0x8B,0xF1,0x57,0x8B,0x4D,0x08,0x8B,0x46,0x58,0x8B,0x9E,0xC0,0x00};
   const unsigned char hexEntry[] = {0x55,0x8B,0xEC,0x83,0xEC,0x2C,0x53,0x56,0x8B,0x75,0x08,0xB9,0x11,0x00,0x00,0x00,0x8B,0xC6};
   const unsigned char armiesEntry[] = {0x55,0x8B,0xEC,0x6A,0xFF,0x68,0xE8,0x42,0x63,0x00,0x64,0xA1,0x00,0x00,0x00,0x00,0x50,0x64};
   const unsigned char spellEntry[] = {0x55,0x8B,0xEC,0x8B,0x45,0x08,0x83,0xF8,0x3B,0x75,0x15,0x8B,0x45,0x14,0x8B,0x55,0x10,0x50};
   const unsigned char speedEntry[] = {0x55,0x8B,0xEC,0x51,0x8B,0x91,0x70,0x02,0x00,0x00,0x8B,0x81,0xC4,0x00,0x00,0x00,0x85,0xD2,0x89,0x45};
   const unsigned char resetEntry[] = {0x55,0x8B,0xEC,0x83,0xEC,0x14,0x53,0x56,0x8B,0xF1,0x57,0xC7,0x45,0xFC,0x08,0x00,0x00,0x00};
   if (!Chain(0x5A83A0, chanceEntry)) return false;
   if (!Chain(0x443DB0, damageEntry)) return false;
   if (!Chain(0x5A4480, hexEntry)) return false;
   if (!Chain(0x5A4A00, armiesEntry)) return false;
   if (!Chain(0x5A4C30, spellEntry)) return false;
   if (!Chain(0x4489F0, speedEntry)) return false;
   if (!Chain(0x43D2E0, resetEntry)) return false;
   int corePosition = 0;
   for (Patch* patch = patcher->GetFirstPatchAt(0x5A83A0); patch; patch = patch->GetAppliedAfter(), ++corePosition)
      if (patch->GetOwner() && std::strcmp(patch->GetOwner(), "HD.Plugin.H3.NewSpells") == 0)
      { coreChance = static_cast<HiHook*>(patch); break; }
   if (!coreChance) return false;
   const unsigned char deathCall[] = {0xE8,0xD3,0x02,0x00,0x00};
   if (patcher->GetFirstPatchAt(0x443F58) || std::memcmp(reinterpret_cast<void*>(0x443F58), deathCall, 5)) return false;
   owner = patcher->CreateInstance("HD.Plugin.H3.NewSpellsExpansion.Blizzard");
   if (!owner) return false;
   chanceHook = owner->CreateHiHook(0x5A83A0, SPLICE_, EXTENDED_, THISCALL_, reinterpret_cast<void*>(WorkChance));
   if (!chanceHook) return false;
   chanceHook->ApplyInsert(corePosition + 1);
   if (!Keep(chanceHook) || !ChanceChain() ||
       !Keep(owner->WriteHiHook(0x443DB0, SPLICE_, EXTENDED_, THISCALL_, Damage)) ||
       !Keep(owner->WriteHiHook(0x5A4480, SPLICE_, EXTENDED_, THISCALL_, MarkHexes)) ||
       !Keep(owner->WriteHiHook(0x5A4A00, SPLICE_, EXTENDED_, THISCALL_, MarkArmies)) ||
       !Keep(owner->WriteHiHook(0x5A4C30, SPLICE_, EXTENDED_, THISCALL_, MarkSpell)) ||
       !Keep(owner->WriteHiHook(0x4489F0, SPLICE_, EXTENDED_, THISCALL_, EffectiveSpeed)) ||
       !Keep(owner->WriteHiHook(0x43D2E0, SPLICE_, EXTENDED_, THISCALL_, ResetStack)) ||
       !Keep(owner->WriteHiHook(0x443F58, CALL_, EXTENDED_, THISCALL_, DeathRemoval)))
   { Shutdown(); return false; }
   ready = true;
   return true;
}
void Shutdown()
{
   ready = false;
   while (hookCount)
   {
      Patch* hook = hooks[--hookCount];
      if (hook->IsApplied()) hook->Undo();
      if (!hook->IsApplied()) hook->Destroy();
   }
   coreChance = chanceHook = 0;
}
NewSpellsProviderSpellV1 Descriptor(const char* providerKey)
{
   const NewSpellsProviderSpellV1 descriptor = {
      sizeof(NewSpellsProviderSpellV1), 1, SpellId, Capabilities, 0, providerKey, "Blizzard",
      0, 0, Validate, Cast, Apply, OnRound, Remove, CureDispel, Battle, Cast, Erm, Evaluate, 0
   };
   return descriptor;
}
#ifdef NEWSPELLS_BLIZZARD_NATIVE_PROBE
#include "../tests/BlizzardNativeProbe.inl"
#endif
}

#ifdef NEWSPELLS_BLIZZARD_NATIVE_PROBE
extern "C" __declspec(dllexport) int __stdcall RunBlizzardNativeTests() { return Blizzard::RunNativeProbe(); }
#endif

