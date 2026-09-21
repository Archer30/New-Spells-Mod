#define _CRT_SECURE_NO_WARNINGS
#define _SECURE_SCL 0
#define _HAS_ITERATOR_DEBUGGING 0
//#define NEWSPELLS_DEBUG

#pragma warning(disable : 4005)
#pragma warning(disable : 4010)

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>
#include "math.h"
#include "ERA\era.h"

#include "HotA\homm3.h"
#include "HoMM3API.h"

#define pGame (*(Game**)0x699538)
#define pAdventureManager (*(AdventureManager**)0x6992B8)
#define pCombatManager (*(CombatManager**)0x699420)
#define pWindowManager (*(WindowManager**)0x6992D0)
#define pMouseManager (*(MouseManager**)0x6992B0)
#define o_ComboArtInfo (*(_ComboArtInfo_**)0x660B6C)
#define o_MagicAnim ((_MagicAnim_*)0x641E18)
#define bCreatureVanish (((bool(*)[20])&pCombatManager->Field<bool>(0x13438)))
#define massSpellTarget (((bool(*)[20])&pCombatManager->Field<bool>(0x547C)))

#define CeilDiv(x, y) ((((x) - 1) / (y)) + 1)

// VC6/VC9 bitsets used 32-bit words. Modern MSVC uses 64-bit words for these
// sizes, changing the ABI of objects exchanged with heroes3.exe.
template<std::size_t BitCount>
struct exe_bitset
{
   static const std::size_t WordCount = (BitCount + 31) / 32;
   std::uint32_t words[WordCount];

   std::size_t size() const
   {
      return BitCount;
   }

   bool test(std::size_t bit) const
   {
      return bit < BitCount &&
         (words[bit / 32] & (std::uint32_t(1) << (bit % 32))) != 0;
   }

   bool operator[](std::size_t bit) const
   {
      return test(bit);
   }

   exe_bitset& set(std::size_t bit, bool value = true)
   {
      if (bit < BitCount)
      {
         const std::uint32_t mask = std::uint32_t(1) << (bit % 32);
         if (value)
            words[bit / 32] |= mask;
         else
            words[bit / 32] &= ~mask;
      }
      return *this;
   }
};

typedef exe_bitset<ORIG_SPELLS_NUM> _SpellBitset70_;
typedef exe_bitset<SPELLS_MAX> _SpellBitset_;

static_assert(sizeof(_SpellBitset70_) == 0x0C, "Heroes III 70-bit spell mask ABI mismatch");
static_assert(sizeof(_SpellBitset_) == SPELLS_MAX / 8, "Extended spell mask ABI mismatch");
static_assert(std::is_trivially_copyable<_SpellBitset_>::value,
   "Executable spell masks must remain trivial binary views");
static_assert(sizeof(_Spell_) == 0x88, "WoG spell-record ABI mismatch");
static_assert(offsetof(_Spell_, wav_name) == 0x04, "Spell sound-pointer offset mismatch");
static_assert(offsetof(_Spell_, name) == 0x10, "Spell name-pointer offset mismatch");
static_assert(offsetof(_Spell_, short_name) == 0x14, "Spell short-name offset mismatch");
static_assert(offsetof(_Spell_, description) == 0x78, "Spell description offset mismatch");
static_assert(offsetof(hero, in_spellbook) == 0x3EA, "Hero spellbook ABI mismatch");

// Structures and classes
struct _ComboArtInfo_ {
   int index;
   exe_bitset<160> parts;

   bool HasPart(std::size_t artifactId) const
   {
      return parts.test(artifactId);
   }
};

static_assert(sizeof(_ComboArtInfo_) == 0x18, "Combination-artifact table ABI mismatch");
static_assert(offsetof(_ComboArtInfo_, parts) == 0x04,
   "Combination-artifact table field offset mismatch");

struct _MagicAnim_ {
   char* defName;
   char* name;
   int type;
};

struct _BookSpell_
{
   int id;
   int school_flags;
   int school_level;
};

struct Game : public _GameMgr_
{
   inline int getCurrentDay()
   {
	  return curr_day_ix + (curr_week_ix - 1) * 7 + (curr_month_ix - 1) * 28;
   }
   inline bool SpellDisabled(enum SpellID spell)
   {
	  return PField<bool>(4)[spell];
   }
   inline void DisableSpell(enum SpellID spell, bool Disabled = true)
   {
	  PField<bool>(4)[spell] = Disabled;
   }
   inline void SetVisibility(const int startX, const int startY, const int z, const int whichPlayer, int range, bool remote_move)
   {
	  CALL_7(void, __thiscall, 0x49CDD0, this, startX, startY, z, whichPlayer, range, remote_move);
   }
   void SetSpellsAvailability();
   int FillShrine(int shrineFlags);
};

struct CombatManager : public _BattleMgr_
{
   inline ::army* GetActiveStack() {
	  if (unk_side < ATTACKER || unk_side > DEFENDER ||
	      current_stack_ix < 0 || current_stack_ix >= 21)
		 return 0;
	  return (::army*)&stack[unk_side][current_stack_ix];
   }
   inline bool hasObstaclePenalty(::army* stack, int hex, int foeHex) {
	  return CALL_4(bool, __thiscall, 0x4670F0, this, stack, hex, foeHex);
   }
   inline bool hasDistancePenalty(const ::army* stack, const ::army* enemy) {
	  return CALL_3(bool, __thiscall, 0x4671E0, this, stack, enemy);
   }
   inline float SpellCastWorkChance(int spellId, int casting_side, const ::army* target_army, bool redirected = false, bool first_target = true, bool creature_spell = false) {
	  return CALL_7(float, __thiscall, 0x5A83A0, this, spellId, casting_side, target_army, redirected, first_target, creature_spell);
   }
   inline void SpellEffect(int effect, ::army* target_army, int iDelay = 100, bool bDoWince = false) {
	  CALL_5(void, __thiscall, 0x4963C0, this, effect, target_army, iDelay, bDoWince);
   }
   inline void AreaEffect(int targetCell, int iSpellType, TSkillMastery mastery, int power) {
	  CALL_5(void, __thiscall, 0x5A4C80, this, targetCell, iSpellType, mastery, power);
   }
   inline void ShowMassSpell(bool (*bEffected)[20], int spellEffect, bool bShowWince)
   {
	  CALL_4(void, __thiscall, 0x5A6AD0, this, bEffected, spellEffect, bShowWince);
   }
   inline bool IsMoatPresent() {
	  return Field<bool>(0x53A8);
   }
   inline bool IsInMoat(int hex, int wallIndex) {
	  return CALL_3(bool, __thiscall, 0x4699A0, this, hex, wallIndex);
   }
   inline int GetAction() {
	  return Field<int>(0x3C);
   }
   bool AbleToSummonElemental(SpellID spell, long side);
};

struct AdventureManager : public _Struct_, public _AdvMgr_
{
   inline void UpdateRadar(bool updateFlag, bool bPartialUpdate, bool view_mines, bool view_heros, bool view_towns) {
	  CALL_6(void, __thiscall, 0x4136F0, this, updateFlag, bPartialUpdate, view_mines, view_heros, view_towns); 
   }
   inline void ProcessHover(int mouseX, int mouseY) {
	  CALL_3(void, __thiscall, 0x40E2C0, this, mouseX, mouseY);
   }
   inline int get_mouse_map_point(type_point& mapPoint) {
	  return CALL_2(int, __thiscall, 0x407A70, this, &mapPoint);
   }
   inline void ShowRoute(int bUpdateScreen, int bReseed, int bChangeButton) {
	  CALL_4(void, __thiscall, 0x418D30, this, bUpdateScreen, bReseed, bChangeButton);
   }
};

struct WindowManager : public _Struct_
{
};

struct HeroWindow : public _Dlg_
{
   inline void DoModal(bool fadeIn) {
	  CALL_2(void, __thiscall, 0x5FFA20, this, fadeIn);
   }
};

struct MouseManager : public _Struct_
{
   inline void SetPointer(int new_frame, int new_set) {
	  CALL_3(void, __thiscall, 0x50CEA0, this, new_frame, new_set);
   }
   inline void ShowPointer(bool force = true) {
	  CALL_2(void, __thiscall, 0x50D7B0, this, force);
   }
   inline void MouseCoords(int& x, int& y) {
	  CALL_2(void, __stdcall, 0x50D700, &x, &y);
   }

   enum EPointerSet
   {
	  SAME_SET = -1,
	  INVALID_SET = -1,
	  DEFAULT_SET = 0,
	  ADVENTURE_SET = 1,
	  COMBAT_SET = 2,
	  SPELL_SET = 3,
	  ARTIFACT_SET = 4,
	  MAX_POINTER_SETS = 5
   };
};

inline float get_spell_work_chance(SpellID spell, TCreatureType target_army_type, const hero* casting_hero, const hero* target_hero)
{
   return CALL_4(float, __fastcall, 0x44A1A0, spell, target_army_type, casting_hero, target_hero);
}

// AI
#pragma pack(push, 4)
struct type_AI_enemy_data
{
   army* enemy;			// 0x00						   
   long damage;			// 0x04
   long count;			// 0x08
   long total_damage;	// 0x0C
						// 0x10
};

struct type_enchant_data
{
   enum SpellID spell;			 // 0x00
   enum TSkillMastery mastery;	 // 0x04
   long power;					 // 0x08
   long duration;				 // 0x0C
   bool check_resistance;		 // 0x10
};

struct type_spell_choice
{
   type_enchant_data data;		 // 0x00
   long target_hex;				 // 0x14
   long second_target_hex;		 // 0x18
   long value;					 // 0x1C
   bool cast_now;				 // 0x20
};

struct type_AI_combat_parameters
{
   long lowest_attack;		   // 0x00
   long lowest_defense;		   // 0x04
   bool kills_only;			   // 0x08
   bool simulated;			   // 0x09
   long friendly_combat_value; // 0x0C
   long enemy_combat_value;	   // 0x10			  
   long awake_friendly_value;  // 0x14
   long awake_enemy_value;	   // 0x18
   long rounds_left;		   // 0x1C
   long our_group;			   // 0x20
   long enemy_group;		   // 0x24
							   // 0x28
   inline void simulate_attack(army* current_army, int& our_hits, army* enemy, int& enemy_hits, bool ranged, int distance = 0) const {
	  CALL_7(void, __thiscall, 0x435600, this, current_army, &our_hits, enemy, &enemy_hits, ranged, distance);
   }
   inline int get_exchange_effect(army* const current_army, army* const enemy, int distance = 0) const {
	  return CALL_4(int, __thiscall, 0x435A10, this, current_army, enemy, distance);
   }
};

class type_AI_spellcaster
{
public:
   type_AI_spellcaster* (__thiscall** VMT)(void*, char); // 0x000
   hero* current_hero;									 // 0x004
   hero* enemy_hero;									 // 0x008
   long our_group;			  							 // 0x00C
   long enemy_group;		   							 //	0x010
   long enemy_can_attack;								 // 0x014
   long can_be_attacked;					 			 // 0x018
   bool win_likely;			  							 // 0x01C
   bool is_creature_spell;								 // 0x01D
   type_AI_combat_parameters estimate;					 // 0x020
   type_AI_spellcaster *enemy_caster;					 // 0x048
   bool owns_enemy_caster;								 // 0x04C
   type_AI_enemy_data melee_enemies[20];				 // 0x050
   type_AI_enemy_data ranged_enemies[20];				 // 0x190
   type_AI_enemy_data worst_enemies[20];				 // 0x2D0
														 // 0x410
   inline int get_attack_skill_value(army* const our_army, army* const enemy, long duration, long bonus) const {
	  return CALL_5(int, __thiscall, 0x437450, this, our_army, enemy, duration, bonus);
   }
   inline int get_defense_skill_value(army* const our_army, long duration, long bonus) const {
	  return CALL_4(int, __thiscall, 0x438560, this, our_army, duration, bonus);
   }
   inline int get_protection_value(army* const our_army, TSpellSchool school, long level, long duration, long amount) const {
	  return CALL_6(int, __thiscall, 0x439330, this, our_army, school, level, duration, amount);
   }
   int get_antimagic_cancel_value(army* current_army, TSkillMastery mastery) const;

   int (type_AI_spellcaster::*type_AI_spellcaster::get_enchantment_function(SpellID spell) const)(army* const, type_enchant_data) const;
   int get_antimagic_value(army* const Army, type_enchant_data data) const;
   int get_poison_value(army* const Army, type_enchant_data data) const;
   int get_disease_value(army* const Army, type_enchant_data data) const;
   int get_age_value(army* const Army, type_enchant_data data) const;
   int get_fear_value(army* const Army, type_enchant_data data) const;
   int get_death_blow_value(army* const Army, type_enchant_data data) const;
   int get_drain_life_value(army* const Army, type_enchant_data data) const;
   int get_toughness_value(army* const Army, type_enchant_data data) const;
   int get_behemoths_claws_value(army* const Army, type_enchant_data data) const;
   int get_hour_of_power_value(army* const Army, type_enchant_data data) const;
   int unimplemented(army* const Army, type_enchant_data data) const;
};

static_assert(sizeof(type_AI_enemy_data) == 0x10, "AI enemy-data ABI mismatch");
static_assert(sizeof(type_enchant_data) == 0x14, "AI enchantment-data ABI mismatch");
static_assert(sizeof(type_spell_choice) == 0x24, "AI spell-choice ABI mismatch");
static_assert(sizeof(type_AI_combat_parameters) == 0x28, "AI combat-parameters ABI mismatch");
static_assert(offsetof(type_AI_spellcaster, enemy_caster) == 0x48,
   "AI spellcaster::enemy_caster ABI mismatch");
static_assert(offsetof(type_AI_spellcaster, melee_enemies) == 0x50,
   "AI spellcaster::melee_enemies ABI mismatch");
static_assert(offsetof(type_AI_spellcaster, ranged_enemies) == 0x190,
   "AI spellcaster::ranged_enemies ABI mismatch");
static_assert(offsetof(type_AI_spellcaster, worst_enemies) == 0x2D0,
   "AI spellcaster::worst_enemies ABI mismatch");
static_assert(sizeof(type_AI_spellcaster) == 0x410, "AI spellcaster ABI mismatch");
#pragma pack(pop)
