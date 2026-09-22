#pragma once

const int NS_DISABLED_SLOTS = 140;
unsigned char nsDisabledEx[SPELLS_MAX - NS_DISABLED_SLOTS];
unsigned char nsDisabledSink;

unsigned char& nsDisabledFlag(void* game, int spell)
{
   if ((unsigned int)spell < NS_DISABLED_SLOTS)
      return ((unsigned char*)game)[4 + spell];
   if ((unsigned int)spell < SPELLS_MAX)
      return nsDisabledEx[spell - NS_DISABLED_SLOTS];
   nsDisabledSink = 0;
   return nsDisabledSink;
}

// Pyramid: "cmp byte ptr [eax + ecx + 4], 0" then jne
int __stdcall nsDisabledPyramid(LoHook* h, HookContext* c)
{
   c->return_address = nsDisabledFlag((void*)c->eax, c->ecx) ? 0x4C170A : 0x4C16F1;
   return NO_EXEC_DEFAULT;
}

// Scholar: "mov bl, byte ptr [edi + eax + 4]"
int __stdcall nsDisabledScholar(LoHook* h, HookContext* c)
{
   c->ebx = (c->ebx & ~0xFF) | nsDisabledFlag((void*)c->edi, c->eax);
   c->return_address = 0x501298;
   return NO_EXEC_DEFAULT;
}

// Mage guild: "mov al, byte ptr [edx + edi + 4]"
int __stdcall nsDisabledMageGuild(LoHook* h, HookContext* c)
{
   c->eax = (c->eax & ~0xFF) | nsDisabledFlag((void*)c->edx, c->edi);
   c->return_address = 0x5BEA56;
   return NO_EXEC_DEFAULT;
}

void __stdcall saveDisabledSpellsEx(Era::TEvent* e)
{
   Era::WriteSavegameSection(sizeof(nsDisabledEx), nsDisabledEx, "NewSpells.DisabledEx");
}

void __stdcall loadDisabledSpellsEx(Era::TEvent* e)
{
   memset(nsDisabledEx, 0, sizeof(nsDisabledEx));
   Era::ReadSavegameSection(sizeof(nsDisabledEx), nsDisabledEx, "NewSpells.DisabledEx");
}

void writeDisabledSpellHooks()
{
   _PI->WriteLoHook(0x4C16EA, nsDisabledPyramid);
   _PI->WriteLoHook(0x501294, nsDisabledScholar);
   _PI->WriteLoHook(0x5BEA52, nsDisabledMageGuild);
   Era::RegisterHandler(saveDisabledSpellsEx, "OnSavegameWrite");
   Era::RegisterHandler(loadDisabledSpellsEx, "OnSavegameRead");
}
