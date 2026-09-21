#pragma once

void nsCancelDurationsEx(army* Army, bool onlyNegative)
{
   if (!nsHasDurationsEx() || !hasValidArmyCoordinates(Army))
      return;
   for (int spell = NS_DURATION_SLOTS; spell < activeSpellCount; ++spell)
      if (nsDuration(Army, spell) > 0 && (!onlyNegative || o_Spell[spell].type < 0))
         Army->CancelIndividualSpell(spell);
}

// New round: durations count down, a spell at 1 ends.
void nsNewRoundDurationsEx(army* Army)
{
   if (!nsHasDurationsEx())
      return;
   for (int spell = NS_DURATION_SLOTS; spell < activeSpellCount; ++spell)
   {
      int& duration = nsDuration(Army, spell);
      if (duration <= 0)
         continue;
      if (duration == 1)
         Army->CancelIndividualSpell(spell);
      else
         --duration;
   }
}

// BattleStack_Die, after the loop: "mov ecx, [esi + 0x84]"
int __stdcall nsDieDurationsEx(LoHook* h, HookContext* c)
{
   nsCancelDurationsEx((army*)c->esi, false);
   return EXEC_DEFAULT;
}

// Dispel on one stack, after the loop over the target (edi).
int __stdcall nsDispelDurationsEx(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->edi;
   if (nsHasDurationsEx() && hasValidArmyCoordinates(Army))
      for (int spell = NS_DURATION_SLOTS; spell < activeSpellCount; ++spell)
         if (nsDuration(Army, spell) > 0 && shouldExecuteExternalDispel(Army, spell))
            Army->CancelIndividualSpell(spell);
   return EXEC_DEFAULT;
}

// Mass Dispel, after the loop over the current stack (esi).
int __stdcall nsMassDispelDurationsEx(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;
   if (nsHasDurationsEx() && hasValidArmyCoordinates(Army))
      for (int spell = NS_DURATION_SLOTS; spell < activeSpellCount; ++spell)
         if (nsDuration(Army, spell) > 0 && shouldExecuteExternalDispel(Army, spell))
            Army->CancelIndividualSpell(spell);
   return EXEC_DEFAULT;
}

bool nsHasActiveDurationEx(army* Army, bool helpfulOnly)
{
   if (!nsHasDurationsEx() || !Army || !hasValidArmyCoordinates(Army))
      return false;
   for (int spell = NS_DURATION_SLOTS; spell < activeSpellCount; ++spell)
      if (nsDuration(Army, spell) > 0 && (!helpfulOnly || o_Spell[spell].type > 0))
         return true;
   return false;
}

void writeDurationLoopHooks()
{
   _PI->WriteLoHook(0x443F66, nsDieDurationsEx);
   _PI->WriteLoHook(0x5A190A, nsDispelDurationsEx);
   _PI->WriteLoHook(0x5A1998, nsMassDispelDurationsEx);
}
