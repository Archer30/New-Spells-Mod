#pragma once

enum NsKind
{
   NS_KIND_NONE,
   NS_KIND_DAMAGE,       // single target, the engine's damage formula and cast path
   NS_KIND_AREA_DAMAGE,  // hex target, the engine's area damage (Fireball shape)
   NS_KIND_ENCHANTMENT,  // timed stat modifiers, mass at expert with SF_EXPERT_MASS_VERSION
   NS_KIND_SUMMON,       // summons a creature stack
   NS_KIND_CUSTOM        // table entry, resources and events only
};

enum NsImmunity
{
   NS_IMMUNE_UNDEAD = 0x1,
   NS_IMMUNE_NON_LIVING = 0x2,
   NS_IMMUNE_SIEGE_WEAPON = 0x4,
   NS_IMMUNE_FIRE_IMMUNE = 0x8
};

const int NS_DATA_SLOTS = NEWSPELLS_EXTERNAL_SPELL_LAST_ID - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID + 1;
const int NS_DATA_LIST_MAX = 16;
char NS_DATA_PROVIDER_KEY[] = "HD.Plugin.H3.NewSpells.Data";

// Attack and defense deltas applied to a real stack, kept to revert them exactly.
struct NsStackMod
{
   int attack;
   int defense;
   bool applied;
};

struct NsDataSpell
{
   int id;
   NsKind kind;
   char spellKey[64];
   int summonCreature;
   bool summonExclusive;    // the side can then summon only this creature, like the elementals
   int creatureMastery;     // creature-ability casts
   int creaturePower;
   int modAttack[4];
   int modDefense[4];
   int modSpeed[4];         // percent, 100 = unchanged
   int modHealth[4];        // percent, 100 = unchanged
   unsigned int immune;
   int immuneCreatures[NS_DATA_LIST_MAX];
   int immuneCreatureCount;
   int cancels[NS_DATA_LIST_MAX];
   int cancelCount;
   char scriptOnCast[64];   // ERM named functions, optional
   char scriptOnApply[64];
   char scriptOnRound[64];
   char scriptOnRemove[64];
   NsStackMod mods[2][21];
};

NsDataSpell nsDataSpells[NS_DATA_SLOTS];
int nsDataSpellCount;

inline NsDataSpell* nsDataSpell(int spell)
{
   if (spell < NEWSPELLS_EXTERNAL_SPELL_FIRST_ID || spell > NEWSPELLS_EXTERNAL_SPELL_LAST_ID)
      return 0;
   NsDataSpell& d = nsDataSpells[spell - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID];
   return d.kind != NS_KIND_NONE ? &d : 0;
}

void nsDataLog(const char* format, ...)
{
   char buffer[512];
   va_list args;
   va_start(args, format);
   vsnprintf(buffer, sizeof(buffer), format, args);
   va_end(args);
   Era::WriteLog("NewSpells", "Data spells", buffer);
}

std::string nsDataKey(int spell, const char* field, int index = ID_NONE)
{
   char key[128];
   if (index == ID_NONE)
      sprintf_s(key, sizeof(key), "NewSpells.DataSpells.%d.%s", spell, field);
   else
      sprintf_s(key, sizeof(key), "NewSpells.DataSpells.%d.%s.%d", spell, field, index);
   return key;
}

int nsDataInt(int spell, const char* field, int defaultValue, int index = ID_NONE)
{
   int value = defaultValue;
   tryGetJsonInt(nsDataKey(spell, field, index), value);
   return value;
}

void nsDataMasteryValues(int spell, const char* field, int (&out)[4], int defaultValue)
{
   for (int mastery = 0; mastery < 4; ++mastery)
      out[mastery] = nsDataInt(spell, field, defaultValue, mastery);
}

void nsDataText(int spell, const char* field, char* out, std::size_t capacity)
{
   char* value = 0;
   out[0] = 0;
   if (tryGetJsonValue(nsDataKey(spell, field), value) && value)
      strncpy(out, value, capacity - 1);
   out[capacity - 1] = 0;
}

int nsDataList(int spell, const char* field, int (&out)[NS_DATA_LIST_MAX])
{
   char* text = 0;
   int count = 0;
   if (!tryGetJsonValue(nsDataKey(spell, field), text) || !text)
      return 0;
   const char* p = text;
   while (*p && count < NS_DATA_LIST_MAX)
   {
      while (*p == ' ' || *p == ',' || *p == ';')
         ++p;
      if (!*p)
         break;
      out[count++] = atoi(p);
      while (*p && *p != ',' && *p != ';')
         ++p;
   }
   return count;
}

NsKind nsParseKind(const char* text)
{
   if (!text) return NS_KIND_NONE;
   if (_stricmp(text, "Damage") == 0) return NS_KIND_DAMAGE;
   if (_stricmp(text, "AreaDamage") == 0) return NS_KIND_AREA_DAMAGE;
   if (_stricmp(text, "Enchantment") == 0) return NS_KIND_ENCHANTMENT;
   if (_stricmp(text, "Summon") == 0) return NS_KIND_SUMMON;
   if (_stricmp(text, "Custom") == 0) return NS_KIND_CUSTOM;
   return NS_KIND_NONE;
}

const char* nsKindName(NsKind kind)
{
   static const char* names[] = {"None", "Damage", "AreaDamage", "Enchantment", "Summon", "Custom"};
   return names[kind];
}

// A record whose id also carries a NewSpells.ExternalSpells declaration belongs to that provider.
void nsLoadDataSpells()
{
   memset(nsDataSpells, 0, sizeof(nsDataSpells));
   nsDataSpellCount = 0;

   for (int spell = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID; spell <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spell)
   {
      char* text = 0;
      if (!tryGetJsonValue(nsDataKey(spell, "kind"), text))
         continue;

      NsKind kind = nsParseKind(text);
      if (kind == NS_KIND_NONE)
      {
         nsDataLog("Spell ID %d: unknown kind %s, allowed kinds are Damage, AreaDamage, Enchantment, Summon and Custom", spell, text);
         continue;
      }

      char externalKey[96];
      sprintf_s(externalKey, sizeof(externalKey), "NewSpells.ExternalSpells.%d.provider", spell);
      if (tryGetJsonValue(externalKey, text))
      {
         nsDataLog("Spell ID %d: an external provider declares this id, the DataSpells record is ignored", spell);
         continue;
      }

      NsDataSpell& d = nsDataSpells[spell - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID];
      d.id = spell;
      nsDataText(spell, "spellKey", d.spellKey, sizeof(d.spellKey));
      if (!d.spellKey[0])
         sprintf_s(d.spellKey, sizeof(d.spellKey), "Spell%d", spell);

      d.summonCreature = nsDataInt(spell, "summonCreature", ID_NONE);
      d.summonExclusive = nsDataInt(spell, "summonExclusive", 1) != 0;
      d.creatureMastery = clampConfigInt(nsDataInt(spell, "creatureMastery", eMasteryNone), eMasteryNone, eMasteryExpert);
      d.creaturePower = clampConfigInt(nsDataInt(spell, "creaturePower", 3), 1, 999);
      nsDataMasteryValues(spell, "modAttack", d.modAttack, 0);
      nsDataMasteryValues(spell, "modDefense", d.modDefense, 0);
      nsDataMasteryValues(spell, "modSpeed", d.modSpeed, 100);
      nsDataMasteryValues(spell, "modHealth", d.modHealth, 100);
      for (int mastery = 0; mastery < 4; ++mastery)
      {
         d.modSpeed[mastery] = clampConfigInt(d.modSpeed[mastery], 1, 1000);
         d.modHealth[mastery] = clampConfigInt(d.modHealth[mastery], 1, 1000);
      }

      d.immune = 0;
      if (nsDataInt(spell, "immuneUndead", 0)) d.immune |= NS_IMMUNE_UNDEAD;
      if (nsDataInt(spell, "immuneNonLiving", 0)) d.immune |= NS_IMMUNE_NON_LIVING;
      if (nsDataInt(spell, "immuneSiegeWeapons", 0)) d.immune |= NS_IMMUNE_SIEGE_WEAPON;
      if (nsDataInt(spell, "immuneFireImmune", 0)) d.immune |= NS_IMMUNE_FIRE_IMMUNE;
      d.immuneCreatureCount = nsDataList(spell, "immuneCreatures", d.immuneCreatures);
      d.cancelCount = nsDataList(spell, "cancels", d.cancels);

      nsDataText(spell, "scriptOnCast", d.scriptOnCast, sizeof(d.scriptOnCast));
      nsDataText(spell, "scriptOnApply", d.scriptOnApply, sizeof(d.scriptOnApply));
      nsDataText(spell, "scriptOnRound", d.scriptOnRound, sizeof(d.scriptOnRound));
      nsDataText(spell, "scriptOnRemove", d.scriptOnRemove, sizeof(d.scriptOnRemove));

      if (kind == NS_KIND_SUMMON && d.summonCreature < 0)
      {
         nsDataLog("Spell ID %d: a Summon spell needs summonCreature", spell);
         continue;
      }

      // An animation that exists only as PNG frames gets a blank def of the given size.
      const int animFrames = nsDataInt(spell, "animationFrames", 0);
      if (animFrames > 0)
      {
         char animDef[64];
         nsDataText(spell, "animationDef", animDef, sizeof(animDef));
         if (!nsRegisterSyntheticDef(animDef, animFrames,
                nsDataInt(spell, "animationWidth", 0), nsDataInt(spell, "animationHeight", 0)))
            continue;
      }

      d.kind = kind;
      ++nsDataSpellCount;
      nsDataLog("Spell ID %d [%s] kind %s", spell, d.spellKey, nsKindName(kind));
   }

   nsDataLog("%d data spell(s)", nsDataSpellCount);
}

unsigned int nsDataCapabilities(const NsDataSpell& d)
{
   unsigned int caps = NEWSPELLS_CAP_COMBAT_CAST | NEWSPELLS_CAP_CREATURE_CAST;
   switch (d.kind)
   {
   case NS_KIND_DAMAGE:
      caps |= NEWSPELLS_CAP_COMBAT_TARGET | NEWSPELLS_CAP_COMBAT_AI;
      break;
   case NS_KIND_AREA_DAMAGE:
      caps |= NEWSPELLS_CAP_COMBAT_AI;
      break;
   case NS_KIND_ENCHANTMENT:
      caps |= NEWSPELLS_CAP_COMBAT_TARGET | NEWSPELLS_CAP_STATUS_APPLY | NEWSPELLS_CAP_STATUS_ROUND |
         NEWSPELLS_CAP_STATUS_REMOVE | NEWSPELLS_CAP_CURE_DISPEL | NEWSPELLS_CAP_ERM_CAST |
         NEWSPELLS_CAP_COMBAT_AI;
      break;
   case NS_KIND_SUMMON:
      caps |= NEWSPELLS_CAP_COMBAT_AI;
      break;
   default:
      char flagsKey[64];
      sprintf_s(flagsKey, sizeof(flagsKey), "era.spells.%d.flags", d.id);
      if (getJsonInt(flagsKey, 0) & SF_SINGLE_TARGET)
         caps |= NEWSPELLS_CAP_COMBAT_TARGET;
      break;
   }
   return caps;
}

// The declaration an external provider would have written in NewSpells.ExternalSpells.
bool nsDataSpellDeclaration(int spell, char*& providerKey, char*& spellKey, char*& kind, int& capabilities)
{
   static char combat[] = "combat";
   NsDataSpell* d = nsDataSpell(spell);
   if (!d)
      return false;
   providerKey = NS_DATA_PROVIDER_KEY;
   spellKey = d->spellKey;
   kind = combat;
   capabilities = (int)nsDataCapabilities(*d);
   return true;
}

bool nsDataImmune(const NsDataSpell& d, const army* Army)
{
   if (!Army)
      return false;
   const unsigned long flags = Army->sMonInfo.attributes;
   if ((d.immune & NS_IMMUNE_UNDEAD) && (flags & CF_UNDEAD))
      return true;
   if ((d.immune & NS_IMMUNE_NON_LIVING) && !(flags & CF_ALIVE))
      return true;
   if ((d.immune & NS_IMMUNE_SIEGE_WEAPON) && (flags & CF_SIEGE_WEAPON))
      return true;
   if ((d.immune & NS_IMMUNE_FIRE_IMMUNE) && (flags & CF_IMMUNE_TO_FIRE_SPELLS))
      return true;
   for (int i = 0; i < d.immuneCreatureCount; ++i)
      if (d.immuneCreatures[i] == Army->armyType)
         return true;
   return false;
}

int nsDataMasteryOf(const army* Army, int spell)
{
   const int mastery = activeSpellMastery[Army->group][Army->index][spell];
   return mastery >= eMasteryNone && mastery <= eMasteryExpert ? mastery : eMasteryNone;
}

double nsDataHealthMul(army* Army)
{
   double mul = 1.0;
   if (!hasValidArmyCoordinates(Army))
      return mul;
   for (int i = 0; i < NS_DATA_SLOTS; ++i)
   {
      const NsDataSpell& d = nsDataSpells[i];
      if (d.kind != NS_KIND_ENCHANTMENT || nsDuration(Army, d.id) <= 0)
         continue;
      mul *= d.modHealth[nsDataMasteryOf(Army, d.id)] / 100.0;
   }
   return mul;
}

int nsDataSpeedPercent(army* Army)
{
   int percent = 100;
   if (!hasValidArmyCoordinates(Army))
      return percent;
   for (int i = 0; i < NS_DATA_SLOTS; ++i)
   {
      const NsDataSpell& d = nsDataSpells[i];
      if (d.kind != NS_KIND_ENCHANTMENT || nsDuration(Army, d.id) <= 0)
         continue;
      const int p = d.modSpeed[nsDataMasteryOf(Army, d.id)];
      if (p < percent)
         percent = p;
   }
   return percent;
}

int nsDataSummonCreature(int spell)
{
   NsDataSpell* d = nsDataSpell(spell);
   return d && d->kind == NS_KIND_SUMMON ? d->summonCreature : ID_NONE;
}
