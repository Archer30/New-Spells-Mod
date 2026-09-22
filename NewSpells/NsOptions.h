#pragma once

bool nsCumulativeUnicornAura;
bool nsQueueFix;
int nsDisabledSpells[64];
int nsDisabledSpellCount;

void nsReadOptions()
{
   nsScriptEvents = getJsonInt("NewSpells.Config.ScriptEvents", 1) != 0;
   nsCumulativeUnicornAura = getJsonInt("NewSpells.Config.CumulativeUnicornAura", 0) != 0;
   nsQueueFix = getJsonInt("NewSpells.Config.QueueFix", 0) != 0;

   nsDisabledSpellCount = 0;
   char* text = 0;
   if (!tryGetJsonValue("NewSpells.Config.DisabledSpells", text) || !text)
      return;
   const char* p = text;
   while (*p && nsDisabledSpellCount < 64)
   {
      while (*p == ' ' || *p == ',' || *p == ';')
         ++p;
      if (!*p)
         break;
      const int spell = atoi(p);
      if (spell >= 0 && spell < SPELLS_MAX)
         nsDisabledSpells[nsDisabledSpellCount++] = spell;
      while (*p && *p != ',' && *p != ';')
         ++p;
   }
}

void nsApplyDisabledSpells()
{
   if (!pGame)
      return;
   for (int i = 0; i < nsDisabledSpellCount; ++i)
      pGame->DisableSpell(static_cast<SpellID>(nsDisabledSpells[i]));
}

// The exe multiplies the work chance by 0.8 once when the target has any unicorn aura, this
// applies the factor once per aura source. At "fmul 0.8" the chance is in ST(0).
int __stdcall nsUnicornAuraChance(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->edi;
   const int auras = Army ? (int)Army->aura_sources.size() : 0;
   float chance;
   __asm { fstp chance }
   chance *= (float)pow(0.8, auras);
   __asm { fld chance }
   c->return_address = 0x5A88D9;
   return NO_EXEC_DEFAULT;
}

// HD mod battle queue: the displayed speed follows Slow, Disease, Fear, Explosion and the data enchantments.
HMODULE nsHdModule;
const int NS_HD_QUEUE_SITE = 0x22615 + 0xC00;
const int NS_HD_QUEUE_RETURN = 0x2264A + 0xC00;

int __stdcall nsQueueSpeed(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->ecx;
   const int queueLength = *(int*)(c->ebp - 0xC);
   int speed = *(int*)(c->ebp - 0x10);

   if (hasValidArmyCoordinates(Army))
   {
      int minSpeedMod = 100;
      if (Army->spellInfluence[SPELL_SLOW] > queueLength && slowSpell[Army->group][Army->index].speedMod < minSpeedMod)
         minSpeedMod = slowSpell[Army->group][Army->index].speedMod;
      if (Army->spellInfluence[SPELL_DISEASE] > queueLength && diseaseSpell[Army->group][Army->index].speedMod < minSpeedMod)
         minSpeedMod = diseaseSpell[Army->group][Army->index].speedMod;
      if (Army->spellInfluence[SPELL_FEAR] > queueLength && fearSpell[Army->group][Army->index].speedMod < minSpeedMod)
         minSpeedMod = fearSpell[Army->group][Army->index].speedMod;
      if (minSpeedMod < 100)
         speed = (int)(speed * minSpeedMod / 100.0f);

      const int percent = nsDataSpeedPercent(Army);
      if (percent != 100)
         speed = max(speed * percent / 100, 1);

      if (explosionSpell[Army->group][Army->index].speedPenalty)
         speed += 1;
   }

   *(int*)(c->ebp - 0x10) = speed;
   c->return_address = (int)nsHdModule + NS_HD_QUEUE_RETURN;
   return NO_EXEC_DEFAULT;
}

void nsInstallOptions()
{
   if (nsCumulativeUnicornAura)
   {
      const unsigned char fmul[] = {0xD8, 0x0D, 0xAC, 0xB8, 0x63, 0x00};
      if (memcmp((void*)0x5A88D3, fmul, sizeof(fmul)) == 0)
         _PI->WriteLoHook(0x5A88D3, nsUnicornAuraChance);
      else
         Era::WriteLog("NewSpells", "Options", "Unexpected code at 0x5A88D3, cumulative unicorn aura not installed");
   }

   if (nsQueueFix)
   {
      nsHdModule = GetModuleHandleA("HD_SOD.dll");
      const unsigned char site[] = {0x8B, 0x85, 0xE4, 0xFE, 0xFF, 0xFF, 0x66, 0x8B, 0x4A, 0x30};
      const unsigned char back[] = {0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x01};
      if (nsHdModule &&
          memcmp((char*)nsHdModule + NS_HD_QUEUE_SITE, site, sizeof(site)) == 0 &&
          memcmp((char*)nsHdModule + NS_HD_QUEUE_RETURN, back, sizeof(back)) == 0)
         _PI->WriteLoHook((int)nsHdModule + NS_HD_QUEUE_SITE, nsQueueSpeed);
      else
         Era::WriteLog("NewSpells", "Options", "HD_SOD.dll is absent or has another layout, queue fix not installed");
   }
}
