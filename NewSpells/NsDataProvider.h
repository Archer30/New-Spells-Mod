#pragma once

NewSpellsProviderSpellV1 nsDataDescriptors[NS_DATA_SLOTS];
NewSpellsProviderBatchV1 nsDataBatch;
bool nsDataProviderRegistered;

bool nsDataContextValid(const void* context, unsigned int size, unsigned int abiVersion, unsigned int expected)
{
   return context && size >= expected && abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1;
}

// Same chain the built-in health spells use, with the data multipliers appended.
void nsDataRecomputeHealth(army* Army)
{
   double fullHealth = (double)Army->origHitPoints * Army->poison_penalty;

   if (Army->spellInfluence[SPELL_AGE])
      fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;
   if (Army->spellInfluence[SPELL_TOUGHNESS])
      fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;
   if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
      fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
   fullHealth *= nsDataHealthMul(Army);

   const int health = normalizedStackHealth(fullHealth);
   Army->sMonInfo.hitPoints = health;
   if (health - 1 < Army->residualDamage)
      Army->residualDamage = health - 1;
}

// AI simulations work on copies, their deltas are not tracked and come from the mastery.
void nsDataRevertMods(NsDataSpell& d, army* Army)
{
   if (!isRealArmy(Army))
   {
      const int mastery = nsDataMasteryOf(Army, d.id);
      Army->sMonInfo.attackSkill = max(Army->sMonInfo.attackSkill - d.modAttack[mastery], 0);
      Army->sMonInfo.defenseSkill = max(Army->sMonInfo.defenseSkill - d.modDefense[mastery], 0);
      return;
   }

   NsStackMod& mod = d.mods[Army->group][Army->index];
   if (!mod.applied)
      return;
   Army->sMonInfo.attackSkill -= mod.attack;
   Army->sMonInfo.defenseSkill -= mod.defense;
   mod.attack = 0;
   mod.defense = 0;
   mod.applied = false;
}

// A refresh with a higher mastery replaces the deltas, a stat never drops below 0.
void nsDataApplyMods(NsDataSpell& d, army* Army, int mastery)
{
   if (mastery < eMasteryNone || mastery > eMasteryExpert)
      mastery = eMasteryNone;
   const bool real = isRealArmy(Army);
   if (real)
      nsDataRevertMods(d, Army);

   int attack = d.modAttack[mastery];
   int defense = d.modDefense[mastery];
   if (Army->sMonInfo.attackSkill + attack < 0)
      attack = -Army->sMonInfo.attackSkill;
   if (Army->sMonInfo.defenseSkill + defense < 0)
      defense = -Army->sMonInfo.defenseSkill;
   Army->sMonInfo.attackSkill += attack;
   Army->sMonInfo.defenseSkill += defense;

   if (real)
   {
      NsStackMod& mod = d.mods[Army->group][Army->index];
      mod.attack = attack;
      mod.defense = defense;
      mod.applied = true;
   }
   nsDataRecomputeHealth(Army);
}

void nsDataClearMods()
{
   for (int i = 0; i < NS_DATA_SLOTS; ++i)
      memset(nsDataSpells[i].mods, 0, sizeof(nsDataSpells[i].mods));
}

int32_t __stdcall nsDataValidateTarget(NewSpellsCombatContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   if (!d)
      return NEWSPELLS_PROVIDER_DENIED;
   army* target = (army*)context->targetStack;
   if (!target)
      return NEWSPELLS_PROVIDER_DENIED;
   return nsDataImmune(*d, target) ? NEWSPELLS_PROVIDER_DENIED : NEWSPELLS_PROVIDER_COMMITTED;
}

int32_t __stdcall nsDataCastCombat(NewSpellsCombatContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   if (!d || !pCombatManager || context->mastery < eMasteryNone || context->mastery > eMasteryExpert)
      return NEWSPELLS_PROVIDER_DENIED;

   switch (d->kind)
   {
   case NS_KIND_AREA_DAMAGE:
      pCombatManager->AreaEffect(context->targetHex, d->id, (TSkillMastery)context->mastery, context->spellPower);
      return NEWSPELLS_PROVIDER_COMMITTED;
   case NS_KIND_SUMMON:
      if (context->casterSide < ATTACKER || context->casterSide > DEFENDER ||
          !pCombatManager->AbleToSummonElemental((SpellID)d->id, context->casterSide))
         return NEWSPELLS_PROVIDER_DENIED;
      CALL_5(void, __thiscall, 0x5A7390, pCombatManager, d->id, d->summonCreature, context->spellPower, context->mastery);
      return NEWSPELLS_PROVIDER_COMMITTED;
   case NS_KIND_CUSTOM:
      return NEWSPELLS_PROVIDER_COMMITTED;
   default:
      return NEWSPELLS_PROVIDER_DENIED;
   }
}

int32_t __stdcall nsDataStatusApply(NewSpellsStatusContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   army* Army = (army*)context->stack;
   if (!d || d->kind != NS_KIND_ENCHANTMENT || !hasValidArmyCoordinates(Army))
      return NEWSPELLS_PROVIDER_DENIED;

   if (!isRealArmy(Army) || !d->mods[Army->group][Army->index].applied)
      for (int i = 0; i < d->cancelCount; ++i)
         if (d->cancels[i] != d->id && nsDuration(Army, d->cancels[i]) > 0)
            Army->CancelIndividualSpell(d->cancels[i]);

   nsDataApplyMods(*d, Army, context->mastery);
   return NEWSPELLS_PROVIDER_COMMITTED;
}

int32_t __stdcall nsDataStatusRound(NewSpellsStatusContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   army* Army = (army*)context->stack;
   if (!d || !hasValidArmyCoordinates(Army))
      return NEWSPELLS_PROVIDER_DENIED;
   if (isRealArmy(Army))
      nsCallErm(d->scriptOnRound, d->id, Army->group, Army->index, context->mastery, context->duration, 0);
   return NEWSPELLS_PROVIDER_COMMITTED;
}

int32_t __stdcall nsDataStatusRemove(NewSpellsStatusContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   army* Army = (army*)context->stack;
   if (!d || !hasValidArmyCoordinates(Army))
      return NEWSPELLS_PROVIDER_DENIED;
   nsDataRevertMods(*d, Army);
   nsDataRecomputeHealth(Army);
   return NEWSPELLS_PROVIDER_COMMITTED;
}

int32_t __stdcall nsDataCureOrDispel(NewSpellsStatusContextV1* context)
{
   return NEWSPELLS_PROVIDER_COMMITTED;
}

// A creature ability casts through the engine, like the built-in Fear and Poison.
int32_t __stdcall nsDataCreatureCast(NewSpellsCombatContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   if (!d || !pCombatManager)
      return NEWSPELLS_PROVIDER_DENIED;
   pCombatManager->CastSpell(d->id, context->targetHex, 1, -1, d->creatureMastery, d->creaturePower);
   return NEWSPELLS_PROVIDER_COMMITTED;
}

// BM:G wrote the duration (spellPower) and mastery, the modifiers follow them.
int32_t __stdcall nsDataErmCast(NewSpellsCombatContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   army* Army = (army*)context->targetStack;
   if (!d || !hasValidArmyCoordinates(Army))
      return NEWSPELLS_PROVIDER_DENIED;
   if (d->kind == NS_KIND_ENCHANTMENT && context->spellPower > 0)
      nsDataApplyMods(*d, Army, context->mastery);
   return NEWSPELLS_PROVIDER_COMMITTED;
}

int nsDataSummonCount(const NsDataSpell& d, int mastery, int power)
{
   __int64 count = (__int64)o_Spell[d.id].effect[mastery] * power;
   if (count < 1)
      count = 1;
   if (count > 100000)
      count = 100000;
   return (int)count;
}

// Enchantments: attack through the engine's skill valuation, the rest as fractions of the
// stack's combat value. Summons: creature AI value times the stack size.
int32_t __stdcall nsDataEvaluateCombatAi(NewSpellsAiContextV1* context)
{
   if (!nsDataContextValid(context, context ? context->size : 0, context ? context->abiVersion : 0, sizeof(*context)))
      return NEWSPELLS_PROVIDER_DENIED;
   NsDataSpell* d = nsDataSpell(context->spellId);
   type_AI_spellcaster* caster = (type_AI_spellcaster*)context->planner;
   if (!d || !caster || !pCombatManager || context->mastery < eMasteryNone || context->mastery > eMasteryExpert)
      return NEWSPELLS_PROVIDER_DENIED;

   context->score = 0;
   context->castNow = 0;
   const int mastery = context->mastery;

   if (d->kind == NS_KIND_SUMMON)
   {
      if (context->spellPower <= 0 || caster->win_likely ||
          context->casterSide < ATTACKER || context->casterSide > DEFENDER ||
          !pCombatManager->AbleToSummonElemental((SpellID)d->id, context->casterSide))
         return NEWSPELLS_PROVIDER_COMMITTED;
      const int units = nsDataSummonCount(*d, mastery, context->spellPower);
      const int k = max(0, caster->estimate.kills_only ? 1000 : o_pCreatureInfo[d->summonCreature].AI_value);
      const __int64 value = (__int64)k * units;
      context->score = value > INT_MAX ? INT_MAX : (int)value;
      context->castNow = 1;
      return NEWSPELLS_PROVIDER_COMMITTED;
   }

   if (d->kind != NS_KIND_ENCHANTMENT)
      return NEWSPELLS_PROVIDER_COMMITTED;

   army* Army = (army*)context->targetStack;
   if (!hasValidArmyCoordinates(Army) || nsDuration(Army, d->id) > 0 || caster->win_likely)
      return NEWSPELLS_PROVIDER_COMMITTED;

   const int attack = d->modAttack[mastery];
   const int defense = d->modDefense[mastery];
   const int health = d->modHealth[mastery];
   const int speed = d->modSpeed[mastery];
   const bool helps = attack > 0 || defense > 0 || health > 100 || speed > 100;
   const bool harms = attack < 0 || defense < 0 || health < 100 || speed < 100;
   const bool friendly = Army->group == context->casterSide;
   if ((friendly && !helps) || (!friendly && !harms))
      return NEWSPELLS_PROVIDER_COMMITTED;

   const float workChance = pCombatManager->SpellCastWorkChance(d->id, context->casterSide, Army, false, true, caster->is_creature_spell);
   if (workChance <= 0.0f)
      return NEWSPELLS_PROVIDER_COMMITTED;

   const int totalCombatValue = Army->get_total_combat_value(caster->estimate.lowest_attack, caster->estimate.lowest_defense);
   double value = 0.0;

   if (attack)
   {
      const int bonus = attack > 0 ? attack : min(Army->sMonInfo.attackSkill, -attack);
      if (bonus > 0)
         value += caster->get_attack_skill_value(Army, Army->AI_target, context->duration, bonus);
   }
   if (defense)
   {
      const int penalty = min(defense > 0 ? defense : -defense, 19);
      value += totalCombatValue * (1.0 - sqrt(1.0 - penalty * 0.05));
   }
   if (health != 100)
      value += totalCombatValue * (health > 100 ? health - 100 : 100 - health) / 100.0;
   if (speed != 100)
      value += totalCombatValue * (speed > 100 ? speed - 100 : 100 - speed) / 200.0;

   value *= workChance;
   context->score = value > INT_MAX ? INT_MAX : (int)value;
   return NEWSPELLS_PROVIDER_COMMITTED;
}

void nsRegisterDataProvider()
{
   nsDataProviderRegistered = false;
   if (!nsDataSpellCount)
      return;

   unsigned int count = 0;
   for (int i = 0; i < NS_DATA_SLOTS; ++i)
   {
      NsDataSpell& d = nsDataSpells[i];
      if (d.kind == NS_KIND_NONE)
         continue;
      NewSpellsProviderSpellV1& s = nsDataDescriptors[count++];
      memset(&s, 0, sizeof(s));
      s.size = sizeof(s);
      s.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
      s.spellId = d.id;
      s.capabilities = nsDataCapabilities(d);
      s.flags = 0;
      s.providerKey = NS_DATA_PROVIDER_KEY;
      s.spellKey = d.spellKey;
      // The registry accepts a callback only together with its capability bit.
      if (s.capabilities & NEWSPELLS_CAP_COMBAT_TARGET) s.ValidateCombatTarget = nsDataValidateTarget;
      if (s.capabilities & NEWSPELLS_CAP_COMBAT_CAST) s.CastCombat = nsDataCastCombat;
      if (s.capabilities & NEWSPELLS_CAP_STATUS_APPLY) s.OnStatusApply = nsDataStatusApply;
      if (s.capabilities & NEWSPELLS_CAP_STATUS_ROUND) s.OnStatusRound = nsDataStatusRound;
      if (s.capabilities & NEWSPELLS_CAP_STATUS_REMOVE) s.OnStatusRemove = nsDataStatusRemove;
      if (s.capabilities & NEWSPELLS_CAP_CURE_DISPEL) s.OnCureOrDispel = nsDataCureOrDispel;
      if (s.capabilities & NEWSPELLS_CAP_CREATURE_CAST) s.OnCreatureCast = nsDataCreatureCast;
      if (s.capabilities & NEWSPELLS_CAP_ERM_CAST) s.OnErmCast = nsDataErmCast;
      if (s.capabilities & NEWSPELLS_CAP_COMBAT_AI) s.EvaluateCombatAi = nsDataEvaluateCombatAi;
   }

   for (unsigned int i = 0; i < count; ++i)
      if (!validateProviderCallbacks(nsDataDescriptors[i]))
         nsDataLog("Spell ID %d: capabilities 0x%X exceed the runtime capabilities 0x%X",
            nsDataDescriptors[i].spellId, nsDataDescriptors[i].capabilities, providerRuntimeCapabilities);

   nsDataBatch.size = sizeof(nsDataBatch);
   nsDataBatch.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
   nsDataBatch.providerKey = NS_DATA_PROVIDER_KEY;
   nsDataBatch.spellCount = count;
   nsDataBatch.spells = nsDataDescriptors;
   nsDataProviderRegistered = registerProviderBatch(&nsDataBatch) != 0;
   nsDataLog(nsDataProviderRegistered ? "registered %d data spell(s)" : "registration of %d data spell(s) was rejected", count);
}

// Only the AI evaluator of the damage kinds differs from the table tails.
void nsApplyDataKindTables()
{
   for (int i = 0; i < NS_DATA_SLOTS; ++i)
   {
      const NsDataSpell& d = nsDataSpells[i];
      if (d.kind == NS_KIND_NONE || !isActiveExternalSpell(d.id))
         continue;
      spellIndirectTableB[d.id - SPELL_QUICKSAND] = 17;
      spellIndirectTableF[d.id - SPELL_EARTHQUAKE] =
         d.kind == NS_KIND_DAMAGE || d.kind == NS_KIND_AREA_DAMAGE ? 2 : 9;
   }
}
