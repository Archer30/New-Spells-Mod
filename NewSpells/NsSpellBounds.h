#pragma once

enum NsBoundCond { NS_JL, NS_JB, NS_JA };

// Instructions that sit between the compare and the branch and must run again.
enum NsBoundReplay
{
   NS_REPLAY_NONE,
   NS_REPLAY_STORE_EBP_EAX,      // mov [ebp+disp], eax
   NS_REPLAY_STORE_EDX_EAX4_ESI, // mov [edx+eax*4], esi
   NS_REPLAY_STORE_EAX_ECX,      // mov [eax], ecx
   NS_REPLAY_APPLY_SWITCH,       // mov [ebp+0x10], edi then mov edx, 2
   NS_REPLAY_RESET_SWITCH        // mov [esi+0x194], edx then mov [esi+eax*4+0x24C], edi
};

struct NsBoundSite
{
   int address;         // first byte of the cmp
   int length;          // cmp, replayed instructions and the branch
   HeroSpellReg reg;
   bool word;           // 16-bit compare
   int boundOffset;     // the exe compares against spellCount + boundOffset
   bool walksDurations; // the loop walks army::spellInfluence and stops at its 162 slots
   NsBoundCond cond;
   int target;
   int fallthrough;
   NsBoundReplay replay;
   int replayDisp;
};

const NsBoundSite nsBoundSites[] =
{
   {0x402900,  5, REG_EDI, false,   0, false, NS_JL, 0x4028F7, 0x402905, NS_REPLAY_NONE, 0},               // spell cheat
   {0x41FC8F,  9, REG_EDI, false,   0, false, NS_JL, 0x41FBD9, 0x41FC98, NS_REPLAY_NONE, 0},               // AI area damage value
   {0x427082,  6, REG_EDX, true,    0, false, NS_JL, 0x427033, 0x427088, NS_REPLAY_NONE, 0},               // Eagle Eye after a virtual battle
   {0x471C55,  5, REG_EDI, false,   0, false, NS_JL, 0x471C4C, 0x471C5A, NS_REPLAY_NONE, 0},               // battle cheat
   {0x4864AE,  5, REG_EDI, false,   0, false, NS_JL, 0x48649A, 0x4864B3, NS_REPLAY_NONE, 0},               // campaign hero replaces a placeholder
   {0x4A2741, 12, REG_EAX, false,   0, false, NS_JL, 0x4A269D, 0x4A274D, NS_REPLAY_STORE_EBP_EAX, -0x14},  // Scholar skill
   {0x48A349,  5, REG_ESI, false,   0, false, NS_JL, 0x48A332, 0x48A34E, NS_REPLAY_NONE, 0},               // campaign hero load
   {0x4F5089,  5, REG_ESI, false,   0, false, NS_JL, 0x4F5080, 0x4F508E, NS_REPLAY_NONE, 0},               // cheat menu
   {0x4C170B,  8, REG_EAX, false,   0, false, NS_JL, 0x4C16C7, 0x4C1713, NS_REPLAY_STORE_EBP_EAX, -0x10},  // pyramids
   {0x5012B5,  8, REG_EAX, false,   0, false, NS_JL, 0x50127A, 0x5012BD, NS_REPLAY_STORE_EBP_EAX, 0x0C},   // Scholar map object
   {0x5BEA2A,  5, REG_EDI, false,   0, false, NS_JB, 0x5BEA36, 0x5BEA2F, NS_REPLAY_NONE, 0},               // mage guild
   {0x5BEA6E,  5, REG_EDI, false,   0, false, NS_JL, 0x5BEA2D, 0x5BEA73, NS_REPLAY_NONE, 0},
   {0x5BEAAB,  5, REG_ESI, false,   0, false, NS_JB, 0x5BEAB8, 0x5BEAB0, NS_REPLAY_NONE, 0},
   {0x5BEB46,  8, REG_ESI, false,   0, false, NS_JB, 0x5BEB56, 0x5BEB4E, NS_REPLAY_STORE_EDX_EAX4_ESI, 0},
   {0x5BEB6F, 11, REG_ESI, false,   0, false, NS_JL, 0x5BEC40, 0x5BEB7A, NS_REPLAY_STORE_EAX_ECX, 0},
   {0x5BEBB0,  5, REG_ESI, false,   0, false, NS_JB, 0x5BEBC2, 0x5BEBB5, NS_REPLAY_NONE, 0},
   {0x5BEC19,  8, REG_ESI, false,   0, false, NS_JB, 0x5BEC29, 0x5BEC21, NS_REPLAY_STORE_EDX_EAX4_ESI, 0},
   {0x59EFD7,  9, REG_EAX, false, -11, false, NS_JA, 0x59F96C, 0x59EFE0, NS_REPLAY_NONE, 0},               // SpellChosen switch
   {0x4446E8, 17, REG_ECX, false, -28, false, NS_JA, 0x444D5C, 0x4446F9, NS_REPLAY_APPLY_SWITCH, 0},       // ApplySpell switch
   {0x444260, 22, REG_EAX, false, -46, false, NS_JA, 0x4444E1, 0x444276, NS_REPLAY_RESET_SWITCH, 0},       // CancelIndividualSpell switch
   {0x44A257,  9, REG_EAX, false, -18, false, NS_JA, 0x44A416, 0x44A260, NS_REPLAY_NONE, 0},               // spell work chance switch
   {0x43B786,  9, REG_EAX, false, -15, false, NS_JA, 0x43B984, 0x43B78F, NS_REPLAY_NONE, 0},               // AI spell value switch
   {0x443F61,  5, REG_EDI, false,   0, true, NS_JL, 0x443F50, 0x443F66, NS_REPLAY_NONE, 0},               // stack death
   {0x446F01,  5, REG_EDI, false,   0, true, NS_JL, 0x446EE0, 0x446F06, NS_REPLAY_NONE, 0},               // new round
   {0x5A1905,  5, REG_ESI, false,   0, true, NS_JL, 0x5A18F7, 0x5A190A, NS_REPLAY_NONE, 0},               // Dispel
   {0x5A1993,  5, REG_EDI, false,   0, true, NS_JL, 0x5A1985, 0x5A1998, NS_REPLAY_NONE, 0},               // mass Dispel
   {0x5A84C6,  5, REG_EAX, false,   0, true, NS_JL, 0x5A84BD, 0x5A84CB, NS_REPLAY_NONE, 0},               // magic resistance
   {0x5A852D,  5, REG_EAX, false,   0, true, NS_JL, 0x5A8519, 0x5A8532, NS_REPLAY_NONE, 0},
   {0x535429,  5, REG_ESI, false,   0, false, NS_JL, 0x535408, 0x53542E, NS_REPLAY_NONE, 0},               // RMG spell scrolls
};

int nsBoundSitesInstalled;
bool nsBoundHooksAttempted;

const NsBoundSite* nsFindBoundSite(int address)
{
   for (std::size_t i = 0; i < sizeof(nsBoundSites) / sizeof(NsBoundSite); ++i)
      if (nsBoundSites[i].address == address)
         return &nsBoundSites[i];
   return 0;
}

int __stdcall nsSpellBound(LoHook* h, HookContext* c)
{
   const NsBoundSite* site = nsFindBoundSite(h->GetAddress());
   if (!site)
      return EXEC_DEFAULT;

   int value = hookReg(c, site->reg);
   if (site->word)
      value = (short)value;
   int bound = (site->walksDurations ? nsDurationLoopCount() : activeSpellCount) + site->boundOffset;

   switch (site->replay)
   {
   case NS_REPLAY_STORE_EBP_EAX:
      *(int*)(c->ebp + site->replayDisp) = c->eax;
      break;
   case NS_REPLAY_STORE_EDX_EAX4_ESI:
      *(int*)(c->edx + c->eax * 4) = c->esi;
      break;
   case NS_REPLAY_STORE_EAX_ECX:
      *(int*)c->eax = c->ecx;
      break;
   case NS_REPLAY_APPLY_SWITCH:
      *(int*)(c->ebp + 0x10) = c->edi;
      c->edx = 2;
      break;
   case NS_REPLAY_RESET_SWITCH:
      *(int*)(c->esi + 0x194) = c->edx;
      *(int*)(c->esi + c->eax * 4 + 0x24C) = c->edi;
      break;
   default:
      break;
   }

   bool taken;
   switch (site->cond)
   {
   case NS_JL: taken = value < bound; break;
   case NS_JB: taken = (unsigned int)value < (unsigned int)bound; break;
   default:    taken = (unsigned int)value > (unsigned int)bound; break;
   }

   c->return_address = taken ? site->target : site->fallthrough;
   return NO_EXEC_DEFAULT;
}

// Every site starts with "cmp r32, imm8" (83 /7 ib), or "66 83" for the 16-bit one.
bool nsBoundSiteMatches(const NsBoundSite& site)
{
   const unsigned char* p = (const unsigned char*)site.address;
   if (site.word)
      return p[0] == 0x66 && p[1] == 0x83 && (p[2] & 0xF8) == 0xF8;
   return p[0] == 0x83 && (p[1] & 0xF8) == 0xF8;
}

// A site that another plugin already changed keeps the exe's own bound of 70 and is logged.
bool installSpellBoundHooks()
{
   nsBoundHooksAttempted = true;
   for (std::size_t i = 0; i < sizeof(nsBoundSites) / sizeof(NsBoundSite); ++i)
   {
      const NsBoundSite& site = nsBoundSites[i];
      if (!nsBoundSiteMatches(site) || _P->GetLastPatchAt(site.address))
      {
         char text[96];
         sprintf(text, "Unexpected code at 0x%X, spell bound not hooked", site.address);
         Era::WriteLog("NewSpells", "Spell bounds", text);
         continue;
      }
      if (_PI->WriteLoHook(site.address, nsSpellBound))
         ++nsBoundSitesInstalled;
   }

   return nsBoundSitesInstalled == sizeof(nsBoundSites) / sizeof(NsBoundSite);
}
