#pragma once

#pragma pack(push, 4)
struct NsBattleCastEvent
{
   int spell;
   int side;        // 0 attacker, 1 defender
   int targetHex;   // -1 when the spell has no hex target
   int mastery;     // 0..3
   int power;       // spell power used for the cast
   int creature;    // 1 when a creature casts
};

struct NsStackSpellEvent
{
   int spell;
   int side;
   int stackIndex;
   int mastery;
   int applied;     // 1 applied, 0 removed
   int hero;        // hero id or -1
};

struct NsAdventureCastEvent
{
   int spell;
   int hero;
   int mastery;
};
#pragma pack(pop)

const char* NS_EVENT_BATTLE_CAST = "NewSpells.OnBattleCast";
const char* NS_EVENT_STACK_SPELL = "NewSpells.OnStackSpell";
const char* NS_EVENT_ADVENTURE_CAST = "NewSpells.OnAdventureCast";

// The global named functions exist as empty triggers in Data\s\new spells.erm.
bool nsScriptEvents = true;

void nsCallErm(const char* function, int a1, int a2, int a3, int a4 = 0, int a5 = 0, int a6 = 0)
{
   if (!function || !function[0] || !Era::ExecErmCmd)
      return;
   char cmd[256];
   sprintf_s(cmd, sizeof(cmd), "FU(%s):P%d/%d/%d/%d/%d/%d;", function, a1, a2, a3, a4, a5, a6);
   Era::ExecErmCmd(cmd);
}

void nsFireBattleCast(int spell, int side, int targetHex, int mastery, int power, bool creature)
{
   NsBattleCastEvent e = {spell, side, targetHex, mastery, power, creature ? 1 : 0};
   Era::FireEvent(NS_EVENT_BATTLE_CAST, &e, sizeof(e));

   if (nsScriptEvents)
      nsCallErm("NewSpells_OnBattleCast", spell, side, targetHex, mastery, power, e.creature);
   if (NsDataSpell* d = nsDataSpell(spell))
      nsCallErm(d->scriptOnCast, spell, side, targetHex, mastery, power, e.creature);
}

void nsFireStackSpell(army* Army, int spell, int mastery, bool applied, hero* Hero)
{
   NsStackSpellEvent e = {spell, Army->group, Army->index, mastery, applied ? 1 : 0, Hero ? Hero->id : -1};
   Era::FireEvent(NS_EVENT_STACK_SPELL, &e, sizeof(e));

   if (nsScriptEvents)
      nsCallErm("NewSpells_OnStackSpell", spell, e.side, e.stackIndex, mastery, e.applied, e.hero);
   if (NsDataSpell* d = nsDataSpell(spell))
      nsCallErm(applied ? d->scriptOnApply : d->scriptOnRemove, spell, e.side, e.stackIndex, mastery, e.applied, e.hero);
}

void nsFireAdventureCast(hero* Hero, int spell, int mastery)
{
   NsAdventureCastEvent e = {spell, Hero ? Hero->id : -1, mastery};
   Era::FireEvent(NS_EVENT_ADVENTURE_CAST, &e, sizeof(e));

   if (nsScriptEvents)
      nsCallErm("NewSpells_OnAdventureCast", spell, e.hero, mastery);
   if (NsDataSpell* d = nsDataSpell(spell))
      nsCallErm(d->scriptOnCast, spell, e.hero, mastery);
}
