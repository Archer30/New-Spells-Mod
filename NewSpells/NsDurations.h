#pragma once

#include <map>

const int NS_DURATION_SLOTS = 162;
const int NS_DURATIONS_EX = SPELLS_MAX - NS_DURATION_SLOTS;

bool isRealArmy(const army* Army);

int nsDurationsEx[2][21][NS_DURATIONS_EX];
struct NsDummyDurations
{
   int values[NS_DURATIONS_EX];
};
// Entries live until the corresponding temporary stack is destroyed. Map nodes
// keep duration references valid while other AI copies are created or released.
std::map<army*, NsDummyDurations> nsDummyDurations;
int nsDurationSink;

NsDummyDurations& nsDummySlot(army* Army)
{
   return nsDummyDurations[Army]; // A new entry is value-initialized to zero.
}

void nsReleaseDummy(army* Army)
{
   nsDummyDurations.erase(Army);
}

inline int& nsDuration(army* Army, int spell)
{
   if ((unsigned int)spell < NS_DURATION_SLOTS)
      return Army->spellInfluence[spell];
   if ((unsigned int)spell >= SPELLS_MAX)
   {
      nsDurationSink = 0;
      return nsDurationSink;
   }
   if (isRealArmy(Army))
      return nsDurationsEx[Army->group][Army->index][spell - NS_DURATION_SLOTS];
   return nsDummySlot(Army).values[spell - NS_DURATION_SLOTS];
}

// The exe loops that walk the array stop here, the hooks below handle the side table.
inline int nsDurationLoopCount()
{
   return activeSpellCount < NS_DURATION_SLOTS ? activeSpellCount : NS_DURATION_SLOTS;
}

inline bool nsHasDurationsEx()
{
   return activeSpellCount > NS_DURATION_SLOTS;
}

void nsClearDurationsEx()
{
   memset(nsDurationsEx, 0, sizeof(nsDurationsEx));
   nsDummyDurations.clear();
}

// AI value: "mov eax, [ebx + ecx*4 + 0x198]" then test eax, eax
int __stdcall nsDurationReadA(LoHook* h, HookContext* c)
{
   c->eax = nsDuration((army*)c->ebx, c->ecx);
   c->return_address = 0x43A39C;
   return NO_EXEC_DEFAULT;
}

// AI value: "mov edx, [ebx + eax*4 + 0x198]" then test edx, edx
int __stdcall nsDurationReadB(LoHook* h, HookContext* c)
{
   c->edx = nsDuration((army*)c->ebx, c->eax);
   c->return_address = 0x43A62A;
   return NO_EXEC_DEFAULT;
}

// CancelIndividualSpell: "cmp [esi + eax*4 + 0x198], edi" then jle
int __stdcall nsDurationCancelCheck(LoHook* h, HookContext* c)
{
   c->return_address = nsDuration((army*)c->esi, c->eax) <= (int)c->edi ? 0x4445BB : 0x44424D;
   return NO_EXEC_DEFAULT;
}

// CanStackReceiveSpell: "mov eax, [esi + ebx*4 + 0x198]" then test eax, eax
int __stdcall nsDurationReceiveCheck(LoHook* h, HookContext* c)
{
   c->eax = nsDuration((army*)c->esi, c->ebx);
   c->return_address = 0x4477AE;
   return NO_EXEC_DEFAULT;
}

// army copy: a temporary copy starts with the source's high durations.
army* __stdcall nsArmyCopy(HiHook* h, army* dest, army* source)
{
   army* result = CALL_2(army*, __thiscall, h->GetDefaultFunc(), dest, source);
   if (nsHasDurationsEx() && source && dest && !isRealArmy(dest))
      memcpy(nsDummySlot(dest).values, isRealArmy(source)
         ? nsDurationsEx[source->group][source->index] : nsDummySlot(source).values, sizeof(int) * NS_DURATIONS_EX);
   return result;
}

void __stdcall nsArmyDestroy(HiHook* h, army* Army)
{
   nsReleaseDummy(Army);
   CALL_1(void, __thiscall, h->GetDefaultFunc(), Army);
}

void writeDurationHooks()
{
   _PI->WriteLoHook(0x43A395, nsDurationReadA);
   _PI->WriteLoHook(0x43A623, nsDurationReadB);
   _PI->WriteLoHook(0x444240, nsDurationCancelCheck);
   _PI->WriteLoHook(0x4477A7, nsDurationReceiveCheck);
   _PI->WriteHiHook(0x437650, SPLICE_, EXTENDED_, THISCALL_, nsArmyCopy);
   _PI->WriteHiHook(0x43D120, SPLICE_, EXTENDED_, THISCALL_, nsArmyDestroy);
}
