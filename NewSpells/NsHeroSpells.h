#pragma once

struct HeroSpellsEx
{
   unsigned char in_spellbook[SPELLS_MAX - ORIG_SPELLS_NUM];
   unsigned char available_spells[SPELLS_MAX - ORIG_SPELLS_NUM];
};

static_assert(offsetof(hero, in_spellbook) == 0x3EA, "hero::in_spellbook must stay at 0x3EA");
static_assert(offsetof(hero, available_spells) == 0x430, "hero::available_spells must stay at 0x430");
static_assert(sizeof(hero) == 0x492, "hero must stay 0x492 bytes");

// The last slot absorbs accesses for a hero whose id is outside 0..HEROES_NUM-1.
HeroSpellsEx heroSpellsEx[HEROES_NUM + 1];
unsigned char heroSpellSink;

// Campaign crossover copies of heroes live in the campaign's lists, not in pGame->hero. Their
// spells are kept per list index and hero id and saved with the campaign.
const int CROSSOVER_LISTS_MAX = 16;
HeroSpellsEx crossoverSpellsEx[CROSSOVER_LISTS_MAX][HEROES_NUM + 1];
HeroSpellsEx crossoverSpellsExSink;

struct HeroVector
{
   int allocator;
   char* first;
   char* last;
   char* end;
};

#define pCrossoverLists ((HeroVector*)((char*)pGame + 0x1F494))

inline bool isGameHero(const hero* Hero)
{
   return pGame && Hero >= (const hero*)pGame->hero && Hero < (const hero*)(pGame->hero + HEROES_NUM);
}

inline int crossoverListCount()
{
   if (!pGame || !pCrossoverLists->first)
      return 0;
   return (pCrossoverLists->last - pCrossoverLists->first) / sizeof(HeroVector);
}

int crossoverListIndex(const hero* Hero)
{
   HeroVector* lists = (HeroVector*)pCrossoverLists->first;
   int count = crossoverListCount();

   for (int i = 0; i < count; ++i)
      if ((const char*)Hero >= lists[i].first && (const char*)Hero < lists[i].last)
         return i;

   return -1;
}

inline HeroSpellsEx& getHeroSpellsEx(const hero* Hero)
{
   unsigned int id = (unsigned int)Hero->id;

   if (id >= HEROES_NUM)
      id = HEROES_NUM;

   if (!isGameHero(Hero))
   {
      int list = crossoverListIndex(Hero);

      if (list >= CROSSOVER_LISTS_MAX)
         return crossoverSpellsExSink;

      if (list >= 0)
         return crossoverSpellsEx[list][id];
   }

   return heroSpellsEx[id];
}

inline unsigned char& heroInSpellbook(hero* Hero, int spell)
{
   if ((unsigned int)spell < ORIG_SPELLS_NUM)
      return ((unsigned char*)Hero->in_spellbook)[spell];

   if ((unsigned int)spell < SPELLS_MAX)
      return getHeroSpellsEx(Hero).in_spellbook[spell - ORIG_SPELLS_NUM];

   heroSpellSink = 0;
   return heroSpellSink;
}

inline unsigned char& heroAvailableSpell(hero* Hero, int spell)
{
   if ((unsigned int)spell < ORIG_SPELLS_NUM)
      return ((unsigned char*)Hero->available_spells)[spell];

   if ((unsigned int)spell < SPELLS_MAX)
      return getHeroSpellsEx(Hero).available_spells[spell - ORIG_SPELLS_NUM];

   heroSpellSink = 0;
   return heroSpellSink;
}

void clearHeroSpellsEx(int heroId)
{
   if ((unsigned int)heroId < HEROES_NUM)
      memset(&heroSpellsEx[heroId], 0, sizeof(HeroSpellsEx));
}

// Same order as the registers in HookContext, so ((int*)context)[reg] addresses a register.
enum HeroSpellReg { REG_EAX, REG_ECX, REG_EDX, REG_EBX, REG_ESP, REG_EBP, REG_ESI, REG_EDI };
enum HeroSpellArray { ARRAY_IN_SPELLBOOK, ARRAY_AVAILABLE, ARRAY_UNKNOWN };

// One "mov r8, byte ptr [heroReg + spellReg + 0x3EA or 0x430]", the array comes from the
// live displacement when the hook is written.
struct HeroSpellSite
{
   int address;
   int length;
   HeroSpellReg heroReg;
   HeroSpellReg spellReg;
   HeroSpellReg destReg;
   HeroSpellArray array;
};

HeroSpellSite heroSpellSites[] =
{
   // Battle AI
   {0x41FBDC, 7, REG_ECX, REG_EDI, REG_EAX}, // CombatManager::get_area_effect()
   {0x425C6F, 7, REG_ECX, REG_EDI, REG_EAX}, // Select and cast a spell
   {0x427036, 7, REG_EBX, REG_EAX, REG_ECX}, // Eagle Eye after a virtual battle
   {0x427041, 7, REG_EDI, REG_EAX, REG_ECX},
   {0x439409, 7, REG_EDX, REG_EBX, REG_EAX}, // Magic resistance spell value
   {0x43C55E, 7, REG_ECX, REG_EDI, REG_EAX}, // Select the best spell
   // Adventure AI
   {0x4329BE, 7, REG_ECX, REG_EDI, REG_EDX}, // Value of a magic sphere
   {0x432CC9, 7, REG_EAX, REG_ESI, REG_ECX}, // Value of a tome
   {0x432CDB, 7, REG_EAX, REG_ESI, REG_ECX},
   {0x4336FC, 7, REG_EAX, REG_EBX, REG_ECX}, // Value of an equipped spell scroll
   {0x433716, 7, REG_EAX, REG_EBX, REG_ECX},
   {0x527AC8, 7, REG_EAX, REG_EDI, REG_ECX}, // Value of a mage guild
   {0x527FA7, 7, REG_ESI, REG_EDI, REG_EAX}, // Value of a spell on the map
   {0x529DD3, 7, REG_ESI, REG_EDI, REG_EAX},
   {0x52A977, 7, REG_ESI, REG_EDI, REG_EAX}, // Value of a pyramid
   {0x52A982, 7, REG_ESI, REG_EDI, REG_EAX},
   {0x52AE19, 7, REG_ECX, REG_EBX, REG_EDX}, // Value of a spell scroll on the map
   {0x52B7A1, 7, REG_ESI, REG_EDX, REG_ECX}, // Value of a town's guild spells
   // Adventure map
   {0x40D979, 7, REG_EAX, REG_EBX, REG_ECX}, // "(Already learned)" hover text
   {0x48649A, 7, REG_ESI, REG_EDI, REG_EAX}, // Campaign hero replaces a placeholder
   {0x4A021A, 7, REG_EDI, REG_ECX, REG_EAX}, // Spells given by an event or Pandora's Box
   {0x4A269D, 7, REG_EDI, REG_EAX, REG_ECX}, // Scholar secondary skill
   {0x4A26A8, 7, REG_EBX, REG_EAX, REG_ECX},
   {0x4A26F1, 7, REG_EBX, REG_EAX, REG_EDX},
   {0x4A4A59, 7, REG_EBX, REG_EDI, REG_EAX}, // Scholar map object
   {0x4A5364, 7, REG_EDI, REG_ESI, REG_EAX}, // Shrine
   {0x574237, 7, REG_EDI, REG_ESI, REG_EAX}, // Seer's Hut reward
   // Spell book
   {0x59CD5B, 7, REG_ECX, REG_EDI, REG_EAX},
};

int heroSpellSitesInstalled;

inline int& hookReg(HookContext* c, HeroSpellReg reg)
{
   return ((int*)c)[reg];
}

inline unsigned char& heroSpellByte(hero* Hero, int spell, HeroSpellArray array)
{
   return array == ARRAY_IN_SPELLBOOK ? heroInSpellbook(Hero, spell) : heroAvailableSpell(Hero, spell);
}

// Executes the hooked "mov r8" in place: the destination keeps its upper bytes, as the exe expects.
int __stdcall readHeroSpell(LoHook* h, HookContext* c)
{
   int address = h->GetAddress();

   for (std::size_t i = 0; i < sizeof(heroSpellSites) / sizeof(HeroSpellSite); ++i)
   {
      const HeroSpellSite& site = heroSpellSites[i];

      if (site.address != address)
         continue;

      hero* Hero = (hero*)hookReg(c, site.heroReg);
      int spell = hookReg(c, site.spellReg);
      int& dest = hookReg(c, site.destReg);

      dest = (dest & ~0xFF) | heroSpellByte(Hero, spell, site.array);
      c->return_address = address + site.length;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

// AI value of an artifact with one spell: eax already holds hero + spell, so both are re-read.
int __stdcall readHeroSpellArtValue(LoHook* h, HookContext* c)
{
   hero* Hero = *(hero**)(c->esi + 4);
   int spell = *(int*)(c->ebp + 8);
   HeroSpellArray array = h->GetAddress() == 0x433000 ? ARRAY_IN_SPELLBOOK : ARRAY_AVAILABLE;

   c->edx = (c->edx & ~0xFF) | heroSpellByte(Hero, spell, array);
   c->return_address = h->GetAddress() + 6;
   return NO_EXEC_DEFAULT;
}

// CombatManager::CastSpell: "cmp byte ptr [edx + ecx + 0x3EA], 0" followed by jne.
int __stdcall castSpellKnownCheck(LoHook* h, HookContext* c)
{
   c->return_address = heroInSpellbook((hero*)c->edx, c->ecx) ? 0x5A02D9 : 0x5A0284;
   return NO_EXEC_DEFAULT;
}

// Hero_Reset writes the starting spell from the hero table to both arrays.
int __stdcall heroResetStartingSpell(LoHook* h, HookContext* c)
{
   hero* Hero = (hero*)c->ebx;

   heroInSpellbook(Hero, c->esi) = 1;
   heroAvailableSpell(Hero, c->esi) = 1;
   c->return_address = 0x4D8B7A;
   return NO_EXEC_DEFAULT;
}

// Hero_Reset clears both arrays before it assigns hero::id, so the id comes from the argument.
int __stdcall heroResetClearSpells(LoHook* h, HookContext* c)
{
   clearHeroSpellsEx(*(short*)(c->ebp + 8));
   return EXEC_DEFAULT;
}

// SetupHero clears both arrays of esi when the map defines the hero's spells.
int __stdcall setupHeroClearSpells(LoHook* h, HookContext* c)
{
   clearHeroSpellsEx(((hero*)c->esi)->id);
   return EXEC_DEFAULT;
}

// Replace_PlaceHolder_With_Hero clears both arrays of ebx, then adds the campaign hero's spells.
int __stdcall placeholderClearSpells(LoHook* h, HookContext* c)
{
   clearHeroSpellsEx(((hero*)c->ebx)->id);
   return EXEC_DEFAULT;
}

// Copies the spells of a game hero that the campaign has just inserted into a crossover list.
void recordCrossoverHero(int list, const hero* value)
{
   if (!value || !isGameHero(value))
      return;

   HeroVector* lists = (HeroVector*)pCrossoverLists->first;
   int index = ((char*)list - (char*)lists) / sizeof(HeroVector);

   if ((char*)list < (char*)lists || index >= crossoverListCount() || index >= CROSSOVER_LISTS_MAX)
      return;

   unsigned int id = (unsigned int)value->id;

   if (id < HEROES_NUM)
      crossoverSpellsEx[index][id] = heroSpellsEx[id];
}

// std::vector<hero>::insert(pos, value)
int __stdcall crossoverInsert(HiHook* h, int list, int pos, hero* value)
{
   int result = CALL_3(int, __thiscall, h->GetDefaultFunc(), list, pos, value);
   recordCrossoverHero(list, value);
   return result;
}

// std::vector<hero>::insert(pos, count, value)
void __stdcall crossoverInsertN(HiHook* h, int list, int pos, int count, hero* value)
{
   CALL_4(void, __thiscall, h->GetDefaultFunc(), list, pos, count, value);
   recordCrossoverHero(list, value);
}

void __stdcall saveHeroSpellsEx(Era::TEvent* e)
{
   int lists = crossoverListCount();

   if (lists > CROSSOVER_LISTS_MAX)
      lists = CROSSOVER_LISTS_MAX;

   Era::WriteSavegameSection(sizeof(heroSpellsEx), &heroSpellsEx, "NewSpells.HeroSpellsEx");
   Era::WriteSavegameSection(sizeof(lists), &lists, "NewSpells.CrossoverLists");

   if (lists > 0)
      Era::WriteSavegameSection(lists * sizeof(crossoverSpellsEx[0]), crossoverSpellsEx, "NewSpells.CrossoverSpellsEx");
}

// A save without the sections comes from an older build, heroes then know no new spells.
void __stdcall loadHeroSpellsEx(Era::TEvent* e)
{
   int lists = 0;

   memset(&heroSpellsEx, 0, sizeof(heroSpellsEx));
   memset(&crossoverSpellsEx, 0, sizeof(crossoverSpellsEx));
   Era::ReadSavegameSection(sizeof(heroSpellsEx), &heroSpellsEx, "NewSpells.HeroSpellsEx");
   Era::ReadSavegameSection(sizeof(lists), &lists, "NewSpells.CrossoverLists");

   if (lists > 0 && lists <= CROSSOVER_LISTS_MAX)
      Era::ReadSavegameSection(lists * sizeof(crossoverSpellsEx[0]), crossoverSpellsEx, "NewSpells.CrossoverSpellsEx");
}

void writeHeroSpellHooks()
{
   for (std::size_t i = 0; i < sizeof(heroSpellSites) / sizeof(HeroSpellSite); ++i)
   {
      HeroSpellSite& site = heroSpellSites[i];
      int displacement = *(int*)(site.address + site.length - 4);

      if (displacement == 0x3EA)
         site.array = ARRAY_IN_SPELLBOOK;
      else if (displacement == 0x430)
         site.array = ARRAY_AVAILABLE;
      else
      {
         char text[96];
         sprintf(text, "Unexpected displacement 0x%X at 0x%X, site not hooked", displacement, site.address);
         Era::WriteLog("NewSpells", "Hero spellbook", text);
         site.array = ARRAY_UNKNOWN;
         continue;
      }

      if (_PI->WriteLoHook(site.address, readHeroSpell))
         ++heroSpellSitesInstalled;
   }

   _PI->WriteLoHook(0x433000, readHeroSpellArtValue);
   _PI->WriteLoHook(0x433024, readHeroSpellArtValue);
   _PI->WriteLoHook(0x5A027A, castSpellKnownCheck);
   _PI->WriteLoHook(0x4D8B6A, heroResetStartingSpell);
   _PI->WriteLoHook(0x4D89AF, heroResetClearSpells);
   _PI->WriteLoHook(0x4D8F27, setupHeroClearSpells);
   _PI->WriteLoHook(0x486473, placeholderClearSpells);
   _PI->WriteHiHook(0x48C940, SPLICE_, EXTENDED_, THISCALL_, crossoverInsert);
   _PI->WriteHiHook(0x48CB50, SPLICE_, EXTENDED_, THISCALL_, crossoverInsertN);

   Era::RegisterHandler(saveHeroSpellsEx, "OnSavegameWrite");
   Era::RegisterHandler(loadHeroSpellsEx, "OnSavegameRead");
}
