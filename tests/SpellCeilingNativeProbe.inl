// Test-only startup probe in a disposable game copy, never packaged, runs after the exe init hook.
namespace CeilingProbe
{
   int checks = 0;

   void Check(bool condition, int line)
   {
      ++checks;
      if (!condition)
      {
         char text[128];
         sprintf_s(text, "FAIL line %d after %d checks", line, checks);
         Era::WriteLog("Ceiling native probe", "Result", text);
         RaiseException(0xE0420214, 0, 0, 0);
      }
   }
#define CEILING_CHECK(x) CeilingProbe::Check(!!(x), __LINE__)

   bool IsJump(int address)
   {
      return *reinterpret_cast<const unsigned char*>(address) == 0xE9;
   }

   // The exe's loader rewrites the name it is given, so it gets a copy.
   int DefFrames(const char* name)
   {
      char buffer[32];
      strcpy_s(buffer, sizeof(buffer), name);
      _Def_* def = _Def_::Load(buffer);
      if (!def)
         return -1;
      const int frames = def->groups_count > 0 && def->groups && def->groups[0]
         ? static_cast<int>(def->groups[0]->frames_count) : -2;
      def->DerefOrDestruct();
      return frames;
   }
}

void ceilingProbeTranslations(const char* stage)
{
   char text[256];
   const char* keys[] = {"era.spells.95.name", "NewSpells.Config.MaxSpellId", "era.spells.150.name", "NewSpells.DataSpells.150.kind"};
   for (int i = 0; i < 4; ++i)
   {
      char* value = Era::tr(keys[i]);
      sprintf_s(text, "%s: %s = %s", stage, keys[i], value ? value : "(null)");
      Era::WriteLog("Ceiling native probe", "Translation", text);
   }
}

void __stdcall ceilingProbeAfterPlugins(Era::TEvent* e) { ceilingProbeTranslations("OnAfterLoadEraPlugins"); }
void __stdcall ceilingProbeAfterWog(Era::TEvent* e) { ceilingProbeTranslations("OnAfterWoG"); }
void __stdcall ceilingProbeBeforeErm(Era::TEvent* e) { ceilingProbeTranslations("OnBeforeErm"); }

void RunCeilingNativeProbe()
{
   char text[256];
   ceilingProbeTranslations("exe init hook");

   // Spell count and byte-bound compare sites.
   CEILING_CHECK(activeSpellCount >= DEFAULT_SPELLS_NUM && activeSpellCount <= WOG_SPELLS_MAX);
   CEILING_CHECK(activeSpellCount == getConfiguredSpellCount());
   CEILING_CHECK(getLiveSpellCount() == activeSpellCount);
   CEILING_CHECK(nsBoundSitesInstalled == sizeof(nsBoundSites) / sizeof(NsBoundSite));
   for (std::size_t i = 0; i < sizeof(nsBoundSites) / sizeof(NsBoundSite); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(nsBoundSites[i].address));
   CEILING_CHECK(nsFindBoundSite(0x402900) && nsFindBoundSite(0x5A852D));

   // Hero spellbook side table sites.
   CEILING_CHECK(heroSpellSitesInstalled == sizeof(heroSpellSites) / sizeof(heroSpellSites[0]));
   for (std::size_t i = 0; i < sizeof(heroSpellSites) / sizeof(heroSpellSites[0]); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(heroSpellSites[i].address));
   const int heroHooks[] = {0x433000, 0x433024, 0x5A027A, 0x4D8B6A, 0x4D89AF, 0x4D8F27, 0x486473, 0x48C940, 0x48CB50};
   for (std::size_t i = 0; i < sizeof(heroHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(heroHooks[i]));

   // Disabled flags above 139 and durations above 161.
   unsigned char game[256] = {};
   nsDisabledFlag(game, 10) = 1;
   nsDisabledFlag(game, NS_DISABLED_SLOTS + 10) = 1;
   CEILING_CHECK(game[4 + 10] == 1 && nsDisabledEx[10] == 1 && nsDisabledFlag(game, 139) == 0);
   nsDisabledFlag(game, NS_DISABLED_SLOTS + 10) = 0;
   const int disabledHooks[] = {0x4C16EA, 0x501294, 0x5BEA52};
   for (std::size_t i = 0; i < sizeof(disabledHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(disabledHooks[i]));

   unsigned char armyBytes[sizeof(army)] = {};
   army* fake = reinterpret_cast<army*>(armyBytes);
   CEILING_CHECK(!isRealArmy(fake));
   nsDuration(fake, 5) = 3;
   nsDuration(fake, NS_DURATION_SLOTS + 8) = 7;
   CEILING_CHECK(fake->spellInfluence[5] == 3 && nsDuration(fake, NS_DURATION_SLOTS + 8) == 7);
   CEILING_CHECK(nsDuration(fake, NS_DURATION_SLOTS + 9) == 0);
   nsReleaseDummy(fake);
   CEILING_CHECK(nsDurationLoopCount() == min(activeSpellCount, NS_DURATION_SLOTS));
   const int durationHooks[] = {0x43A395, 0x43A623, 0x444240, 0x4477A7, 0x437650, 0x43D120, 0x443F66, 0x5A190A, 0x5A1998};
   for (std::size_t i = 0; i < sizeof(durationHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(durationHooks[i]));

   // Chaining hooks and the scroll write inside the artifact updater.
   const int chainedHooks[] = {0x4489F0, 0x4422B0, 0x4D9840, 0x4D988D};
   for (std::size_t i = 0; i < sizeof(chainedHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(chainedHooks[i]));

   // Virtual LOD: the icon sheets come back padded.
   const int lodHooks[] = {0x4FB100, 0x4FACA0, 0x4FB1B0};
   for (std::size_t i = 0; i < sizeof(lodHooks) / sizeof(int); ++i)
      CEILING_CHECK(CeilingProbe::IsJump(lodHooks[i]));
   CEILING_CHECK(CeilingProbe::DefFrames("SpellInt.def") == SPELLS_MAX + 1);
   CEILING_CHECK(CeilingProbe::DefFrames("spells.def") == SPELLS_MAX);
   CEILING_CHECK(CeilingProbe::DefFrames("SpellScr.def") == SPELLS_MAX);
   CEILING_CHECK(CeilingProbe::DefFrames("SpellBon.def") == SPELLS_MAX);

   // Data spells: every loaded record is an active external slot with the engine tables set.
   int activeData = 0;
   for (int i = 0; i < NS_DATA_SLOTS; ++i)
   {
      const NsDataSpell& d = nsDataSpells[i];
      if (d.kind == NS_KIND_NONE)
         continue;
      CEILING_CHECK(nsDataProviderRegistered);
      CEILING_CHECK(d.id < activeSpellCount);
      CEILING_CHECK(isActiveExternalSpell(d.id));
      CEILING_CHECK(spellIndirectTableB[d.id - SPELL_QUICKSAND] == 17);
      CEILING_CHECK(spellIndirectTableF[d.id - SPELL_EARTHQUAKE] ==
         (d.kind == NS_KIND_DAMAGE || d.kind == NS_KIND_AREA_DAMAGE ? 2 : 9));
      CEILING_CHECK(o_Spell[d.id].name && *o_Spell[d.id].name && o_Spell[d.id].level >= 1);
      char* defName = 0;
      char* animName = 0;
      int animType = 0;
      if (getExternalAnimation(d.id, defName, animName, animType))
         CEILING_CHECK(CeilingProbe::DefFrames(defName) > 0);
      ++activeData;
   }
   CEILING_CHECK(activeData == nsDataSpellCount);

   sprintf_s(text, "PASS: %d checks, spell count %d, %d data spell(s)", CeilingProbe::checks, activeSpellCount, activeData);
   Era::WriteLog("Ceiling native probe", "Result", text);
}
