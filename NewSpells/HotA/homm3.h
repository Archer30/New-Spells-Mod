#pragma once

#include "HoMM3_ids.h"
#include "HoMM3_Base.h"
#include "HoMM3_Res.h"
#include "HoMM3_GUI.h"


//#include <ddraw.h>



#define EXE_VERSION *(_dword_*)0x588479
#define TE 0x001F0E12
#define WOG 0x001EC3A3
#define SOD 0xFFFFE403

// Абсолютный путь к корневой папке игры.
#define MAIN_ABSPATH *(_cstr_)0x698614

//#define o_HEROES_COUNT 156
//#define o_CREARURES_COUNT 150
#define o_HEROES_COUNT (*(_int_*)0x4BD145)
#define o_CREARURES_COUNT (*(_int_*)0x5C8047)




struct _Army_;
struct _CreatureInfo_;
struct _HeroInfo_;
struct _TurnTimeStruct_;
struct _MapItem_;
struct _Artifact_;
struct _ArtInfo_;
struct _Hero_;
struct _Player_;
struct _Town_;
struct _MapGlobalEvent_;
struct _BattleAnim_;
// Карта игры.
struct _GameMap_;
// Объект на карте приключений.
struct _Object_;
// Объект - лагерь героя (при загрузке карты).
struct _PlaceHolder_;
// Список героев.
struct _HeroesList_;
// Внешнее жилище на карте.
struct _Dwelling_;

// Настройки зоны генератора случйных карт.
struct _ZoneSettings_;




// Менеджреры игры. Несколько из них ещё не распознаны.
// _bool32_ __cdecl sub_4EE1D0() - инициализация менеджеров. По ней распознан их размер.

// Some Mgr по 0x699550, размер: 16

// Менеджер ввода, управляющий событиями ввода.
struct _InputMgr_; // Размер: 2400
#define o_InputMgr (*(_InputMgr_**)0x699530)

// Менеджер мыши.
struct _MouseMgr_; // Размер: 144
#define o_MouseMgr (*(_MouseMgr_**)0x6992B0)

// Менеджер окна программы.
struct _WndMgr_; // Размер: 96
#define o_WndMgr (*(_WndMgr_**)0x6992D0)

// Менеджер звука.
struct _SoundMgr_; // Размер: 216
#define o_SoundMgr (*(_SoundMgr_**)0x699414)

// Some Mgr по 0x69941C, размер: 2260

// Менеджер игры.
struct _GameMgr_; // Размер: 321488
#define o_GameMgr (*(_GameMgr_**)0x699538)

// Менеджер карты приключений.
struct _AdvMgr_; // Размер: 952
#define o_AdvMgr (*(_AdvMgr_**)0x6992B8)

// Менеджер битвы.
struct _BattleMgr_; // Размер: 82156
#define o_BattleMgr (*(_BattleMgr_**)0x699420)

// Менеджер окна города.
struct _TownMgr_; // Размер: 472
#define o_TownMgr (*(_TownMgr_**)0x69954C)

// Some Mgr по 0x6992D4, размер: 112

// Some Mgr по 0x6992DC, размер: 1










#define o_ScreenBPP (*(_int_*)0x68C8C0)
#define o_ScreenWidth (*(_int_*)0x68C8C4)
#define o_ScreenHeight (*(_int_*)0x68C8C8)
#define o_DD (*(LPDIRECTDRAW*)0x6AAD20)
#define o_DDSurfacePrimary (*(LPDIRECTDRAWSURFACE*)0x6AAD24)
#define o_DDSurfaceBackBuffer (*(LPDIRECTDRAWSURFACE*)0x6AAD28)
#define o_DDSurface6AAD2C (*(LPDIRECTDRAWSURFACE*)0x6AAD2C)
#define o_DDSurface6AAD30 (*(LPDIRECTDRAWSURFACE*)0x6AAD30)
#define o_DDSurface6AAD34 (*(LPDIRECTDRAWSURFACE*)0x6AAD34)
#define o_FullScreenMode (*(_bool_*)0x698808)
#define o_hWnd (*(HWND*)0x699650)

#define HERO_INFO_OFFSET (*(_ptr_*)0x67DCE8)
//#define o_HeroInfo ((_HeroInfo_*)0x679DD0)
#define o_HeroInfo ((_HeroInfo_*)HERO_INFO_OFFSET)
#define o_pHeroInfo (*(_HeroInfo_**)0x67DCE8)


#define o_NetworkGame *(_bool_*)0x69959C
//#define o_CampaignGame *(_bool_*)0x69779C
#define o_ActivePlayer (*(_Player_**)0x69CCFC)
#define o_ActivePlayerID *(_int_*)0x69CCF4
#define o_MeID *(_int_*)0x6995A4

#define o_Market_Hero (*(_Hero_**)0x6AAAE0)
#define o_DirectPlayCOMObject *(_ptr_*)0x69D858

//#define CREATURE_INFO_OFFSET (*(_ptr_*)0x47ADD1)
#define CREATURE_INFO_OFFSET (*(_ptr_*)0x6747B0)
//#define o_CreatureInfo ((_CreatureInfo_*)0x6703B8)
#define o_CreatureInfo ((_CreatureInfo_*)CREATURE_INFO_OFFSET)
#define o_pCreatureInfo (*(_CreatureInfo_**)0x6747B0)

#define o_GameMgrType (*(_int_*)0x698A40)
#define o_QuickBattle (*(_int_*)0x6987CC)
#define o_AutoSolo *(_byte_*)0x691259

#define o_CombatOptionsDlg (*(_Dlg_**)0x694FE0)
#define o_MapWidth *(_dword_*)0x6783C8
#define o_MapHeight *(_dword_*)0x6783CC
#define o_ViewingWorldEarthAirNow *(_dword_*)0x6AACA4
#define o_ViewWorldEarthAirMapCellSize *(_float_*)0x68C708
#define o_MusicVolume (*(_byte_*)0x6987B0)
#define o_TextBuffer ((char*)0x697428)
#define o_TurnTimer ((_TurnTimeStruct_*)0x69D680)
#define o_ScreenLogStruct ((_ptr_)0x69D800)
#define o_ArtInfo (*(_ArtInfo_**)0x660B68)
#define o_RegKeyPath ((_char_*)0x67FE78)
#define o_RegKey_AppPath ((_char_*)0x67FC10)
#define o_RegKey_CDDrive ((_char_*)0x67FC08)

#define o_CurrentDlg (*(_Dlg_**)0x698AC8)

#define o_HeroDlg_Hero (*(_Hero_**)0x698B70)
#define o_HeroDlg_SelStackIndex (*(_int_*)0x697788)


//#define o_BattleAnimation ((_BattleAnim_*)0x641E18)
extern _BattleAnim_* o_BattleAnimation;
extern _int_ BattleAnims_Count;


#define p_NewScenarioDlg (*(_ptr_*)0x69FC44)

#define b_unpack_z(xyz) (((_int16_)(((_dword_)(xyz)) >> 14)) >> 12)
#define b_unpack_y(xyz) (((_int16_)(((_dword_)(xyz)) >> 10)) >> 6)
#define b_unpack_x(xyz) (((_int16_)(((_dword_)(xyz)) << 6)) >> 6)
#define b_pack_xyz(x, y, z) (_dword_)( (((_dword_)(((_word_)(y)) & 0x3FF)) << 16) | ((_dword_)(((_word_)(x)) & 0x3FF)) | (((_dword_)(((_word_)(z)) & 0xF)) << 26) )


// Таблица специализаций героев.
#define Heroes_Spec_Table (_ptr_)0x679C80


// Текущая игра - встроенная кампания.
#define Game_Is_BuiltInCampaing (*(_bool8_*)0x69779C)


// Таблица заклинаний.
#define o_Spell (*(_Spell_**)0x687FA8)


// Минимальное время между кадрами (user).
#define MIN_FRAME_PERIOD 10




// Количество гексов поля боя.
#define BATTLE_HEXES_COUNT 187


// Количество стеков у каждой стороны на поле боя.
#define BATTLE_SIDE_STACKS_COUNT 21


// Количество порядков активных элементов отображения в бою.
#define BATTLE_MAPPING_PRIORITIES_COUNT 8



// Количество элементов замка.
#define CASTLE_ELEMENTS_COUNT 8




// Время ожидания одного кадра отроисовки полёта небаллистического выстрела или заклинания.
#define ShotFrameTime (*(_float_*)0x63B8A0)




// Время, до которого будет производиться ожидание при следующей отрисовке.
// Устанавливается каждой отрисовкой для следующей...
// ...чтобы, если на процессы между ними затратится время, оно автоматически вычлось из времени ожидания.
#define DrawingWaitTime (*(_dword_*)0x6989E8)



// Время следующего проигрыша анимации ожидания в бою.
#define WaitAnimTime (*(_dword_*)0x698A08)



// Массив множителей периодов ожиданий анимации для настройки скорости боя.
#define BattleAnimPeriodFactors ((_float_*)0x63CF7C)

// Настройка скорости боя (индес нужного множителя периодов ожиданий в массиве).
#define Settind_BattleFast (*(_dword_*)0x69883C)


// Временная текстовая переменная. Размер: 768 байт.
#define H3TempStr ((char*)0x697428)


// Пустая переменная.
#define EmptyVar (*(DWORD*)0x691260)


// Пустой звук.
#define EmptySample (*(_Sample_*)0x6992A8)


// Стандартный период анимации.
#define STD_FRAME_PERIOD 100




// Границы перерисовки поля боя.
#define BattleRedraw_Borders (*(_RedrawBorders_*)0x694F68)




// Боевые элементы замка.
#define CastleElements ((_CastleElement_*)0x63BE60)


// Номера гексов крепостной стены по рядам поля боя.
#define CastleWall_Gexes ((_byte_*)0x63BD00)



//NOALIGN struct _XYZ_
//{
// _dword_ z : 6;
// _dword_ y : 10;
// _dword_ x : 10;
//}



NOALIGN struct _Resources_
{
 _int_ wood;
 _int_ mercury;
 _int_ ore;
 _int_ sulfur;
 _int_ crystal;
 _int_ jems;
 _int_ gold;
};

NOALIGN struct _Army_
{
 _int_ type[7];
 _int_ count[7];
 
 // Конструтор.
 inline _Army_* Construct()
 {
   return CALL_1(_Army_*, __thiscall, 0x44A750, this);
 }
 
 // normal
 inline int GetStacksCount() {return CALL_1(int, __thiscall, 0x44A990, this);}
 inline int GetCreaturesCount() {return CALL_1(int, __thiscall, 0x44AA70, this);}
 inline void SwapStackTo(int ix, _Army_* dst_army, int dst_ix) {CALL_4(void, __thiscall, 0x44AA30, this, ix, dst_army, dst_ix);}
 inline void SplitStackByDlgTo(int i, _Army_* dst_army, int dst_i, _bool8_ is_hero, _bool8_ dst_is_hero)
  {CALL_6(void, __thiscall, 0x449B60, this, i, dst_army, dst_i, is_hero, dst_is_hero);}
};

NOALIGN struct _CreatureInfo_
{
 _int_ town;
 _int_ level;
 _char_* sound_name;
 _char_* def_name;
 _int_ flags;
 _char_* name_single;
 _char_* name_plural;
 _char_* specification_description;

 //_int_ cost_wood;
 //_int_ cost_mercury;
 //_int_ cost_ore;
 //_int_ cost_sulfur;
 //_int_ cost_crystal;
 //_int_ cost_jems;
 //_int_ cost_gold;
 _Resources_ cost;

 _int_ fight_value;
 _int_ AI_value;
 _int_ growth;
 _int_ horde_growth;
 _int_ hit_points;
 _int_ speed;
 _int_ attack;
 _int_ defence;
 _int_ damage_min;
 _int_ damage_max;
 _int_ shots;
 _int_ spells_count;
 _int_ advmap_low;
 _int_ advmap_high;
};

NOALIGN struct _HeroInfo_
{
 // Пол (0 - женский, 1 - мужской)
 _dword_ sex;
 _dword_ field_04[8];
 _dword_ army_type[3];
 _ptr_ hps_name;
 _ptr_ hpl_name;
 _byte_ allowed_in_roe; // Разрешён ли в Возрождении Эрафии.
 _byte_ allowed_in_not_roe; // Разрешён ли не в Возрождении Эрафии.
 _byte_ is_campaing; // Кампанейский ли.
 _byte_ field_38[5];
 _ptr_ name;
 _dword_ army_count[6];
};


NOALIGN struct _TurnTimeStruct_
{
 _dword_ last_shown;
 _dword_ turn_start;
 _dword_ turn_limit;
 _dword_ next_shown;
 _dword_ battle_start;

 inline void NewTurn() { CALL_1(void, __thiscall, 0x558130, this); }
};









NOALIGN struct _HString_
{
 _char_* c_str;
 _int_ length;
};
NOALIGN struct _HStringA_
{
 _HString_ h_str;
 _int_ a;
};
NOALIGN struct _HStringF_
{
 _bool8_ is_memory_allocated;
 _byte_ dummy_f1[3];
 _HString_ h_str;
 _int_ size;
};
NOALIGN struct _GlobalEvent_
{
 _HStringF_ message_f;
 _Resources_ resouces;
 _byte_ players_bits;
 _byte_ for_human;
 _byte_ for_ai;
 _word_ day;
 _word_ repeat;
};
NOALIGN struct _MapItem_
{
 _dword_  setup; //+0
 _byte_  land; // +4
 _byte_  land_type;//+5
 _byte_  river;//+6
 _byte_  river_type;//+7
 _byte_  road;//+8
 _byte_  road_type;//+9
 _word_  field_0A;//+10
 _byte_  mirror;//+12
 _byte_  attrib;//+13
 _word_  field_0E_bits;//+14
 _word_  field_10;//+16
 _dword_  draw;//+18
 _dword_  draw_end;//+22
 _dword_  draw_end_2;//+26

 _dword_  object_type;//+30

 _word_  os_type; //+34
 _word_  draw_num;//+36

 _dword_ GetReal__setup() { return CALL_1(_dword_, __thiscall, 0x4FD280, this);}
 _dword_ GetReal__object_type() { return CALL_1(_dword_, __thiscall, 0x4FD220, this);}
 
 // Сделать объект посещённым игроком.
 inline void SetAsVisited(_int_ player_ix)
 {
   CALL_2(void, __thiscall, 0x4FC620, this, player_ix);
 }
 
 // Сделать объект посещённым игроком.
 inline _bool8_ IsVisited(_int_ player_ix)
 {
   return CALL_2(_bool8_, __thiscall, 0x529B70, this, player_ix);
 }
 
 
};



NOALIGN struct _Artifact_
{
 _int_ id;
 _int_ mod;
};

// 32 bytes.
NOALIGN struct _ArtInfo_
{
 char* name;
 _int_ cost;
 _int_ position; // Тип слота, а не номер (например, оба слота кольца - 1 тип)
 _int_ type;
 char* description;
 _int_ supercomposite;
 _int_ part_of_supercomposite;
 _byte_ disabled;
 _byte_ new_spell;
 _byte_ field_1E;
 _byte_ field_1F;
};


// 8 bytes. Слот для артефакта.
NOALIGN struct _ArtSlotInfo_
{
  // +0. Имя слота.
  _cstr_ name;
  
  // +4. Тип слота.
  _int_ type;
};


NOALIGN struct _Hero_
{
 _int16_ x; // +0
 _int16_ y; // +2
 _int16_ z; // +4
 _byte_ visible; // +6
 _dword_ mui_xyz; // +7
 _byte_ field_0B; // +11
  // miu = map_item under hero
 _dword_ miu_object_type; // +12
 _dword_ miu_object_c_flag; // +16
 _dword_ miu_setup; // +20
 _word_ spell_points; // +24
 _dword_ id; // +26
 _dword_ id_wtf; // +30
 _int8_ owner_id; // +34
 _char_ name[13]; // +35
 _dword_ _class; // +48
 _byte_ pic; // +52
 _dword_ aim_x; // +53
 _dword_ aim_y; // +57
 _dword_ aim_z; // +61
 _byte_ field_41[3]; // +65
 _byte_ x_0; // +68
 _byte_ y_0; // +69
 _byte_ run; // +70
 _byte_ field_47; // +71
 _byte_ flags; // +72
 _dword_ movement_points_max; // +73
 _dword_ movement_points; // +77
 _dword_ expa; // +81
 _word_ level; // +85
 _dword_ visited[10]; // +87
 _byte_ field_7F[18]; // +127
 _Army_ army; // +145
 _byte_ second_skill[28]; // +201
 _byte_ second_skill_show[28]; // +229
 _dword_ second_skill_count; // +257
 _dword_ temp_mod_flags; // + 261
 _byte_ field_109[4]; // +265
 _byte_ dd_cast_this_turn; // +269
 _dword_ disguise; // +270
 _dword_ field_112; // +274
 _byte_ d_morale; // +278
 _byte_ field_117[3]; // +279
 _byte_ d_morale_1; // +282
 _byte_ d_luck; // +283
 _byte_ is_sleeping_byte11C; // +284
 _byte_ field_11E[16]; // +284
 _Artifact_ doll_art[19]; // +301
 _byte_ free_add_slots; // +453
 _byte_ locked_slot[14]; // +454
 _Artifact_ backpack_art[64]; // +468
 _byte_ backpack_arts_count; // +980
 _dword_ sex; // +981
 _bool8_ has_biography; // +985
 _HStringF_ biography; // +986
 _byte_ spell[70]; // +1002
 _byte_ spell_level[70]; // +1072
 //_byte_ primary_skill[4];
 _byte_ attack; // +1142
 _byte_ defence; // +1141
 _byte_ power; // +1142
 _byte_ knowledge; // +1143
 _byte_ field_47A[24]; // +1144

 // normal
 void Hide() {CALL_1(void, __thiscall, 0x4D7950, this);}
 void Show(_int_ mapitem_type, _int_ mapitem_setup) {CALL_3(void, __thiscall, 0x4D7840, this, mapitem_type, mapitem_setup);}
 char* Get_className() {return CALL_1(char*, __thiscall, 0x4D91E0, this);}
 int GetLandModifierUnder() {return CALL_1(int, __thiscall, 0x4E5210, this);}

 void GiveArtifact(_Artifact_* art, int a3, int a4) {CALL_4(void, __thiscall, 0x4E32E0, this, art, a3, a4);}
 void GiveArtifact(_int_ art_id, int a3, int a4) 
 {
  _Artifact_ art;
  art.id = art_id;
  art.mod = 0xFFFF;
  GiveArtifact(&art, a3, a4);
 }

 _bool_ DoesHasArtifact(int art_id) {return CALL_2(_bool_, __thiscall, 0x4D9420, this, art_id);}
 _bool_ DoesWearArtifact(int art_id) {return CALL_2(_bool_, __thiscall, 0x4D9460, this, art_id);}
 void RemoveBattleMachine(int creature_id) {CALL_2(void, __thiscall, 0x4D94D0, this, creature_id);}
 int RemoveDollArtifact(int doll_slot_index) {return CALL_2(int, __thiscall, 0x4E2E40, this, doll_slot_index);}
 int AddDollArtifact(_Artifact_* art, int doll_slot_index) {return CALL_3(int, __thiscall, 0x4E2C70, this, art, doll_slot_index);}
 int RemoveBackpackArtifact(int index) {return CALL_2(int, __thiscall, 0x4E2FC0, this, index);}
 int AddBackpackArtifact(_Artifact_* art, int index) {return CALL_3(int, __thiscall, 0x4E3200, this, art, index);}
 
 
 // Получение бонуса к эффекту заклинания за специализацию.
 _int_ GetSpell_Specialisation_Bonuses(_int_ Spell_id, _int_ Creature_level, _int_ BaseMidif)
 {
   return CALL_4(_int_, __thiscall, 0x4E6260, this, Spell_id, Creature_level, BaseMidif);
 }
 
 
 
 //my

 //inline _bool_ DoesHasDollArtifact(int art_id)
 //{
 // for (int i = 0; i < 19; i++)
 // {
 //  if (this->doll_art[i].id == art_id)
 //   return TRUE;
 // }
 // return FALSE;
 //}

 void ShowSpellBookDlg(int a1, int a2, int land_modifier)
 {
  if (this->doll_art[17].id != -1)
  {
   _Dlg_* dlg = (_Dlg_*)o_New(0xD0);
   // create spellbook dlg
   CALL_5(void, __thiscall, 0x59C0F0, dlg, this, a1, a2, land_modifier);
   dlg->Run();
   // destroy spellbook dlg
   CALL_1(void, __thiscall, 0x59CBF0, dlg);
   o_Delete(dlg);
  }
 }
 void ShowSpellBookDlg(int a1, int a2) 
  {ShowSpellBookDlg(a1, a2, this->GetLandModifierUnder());}
};



NOALIGN struct _Player_
{
 _int8_ id;
 _int8_ heroes_count;
 _word_ field_2;
 _int_ selected_hero_id;
 _int_ heroes_ids[8];

 _int_ tavern_heroes[2];

 _byte_ field_30[13];

 _byte_ days_to_kill; //сколько дней до убивания героя (может быть >7)
       //если не FF, то всегда выдает сообщение

 _int8_ towns_count;
 _int8_ selected_town_id;
 _int8_ towns_ids[48];
 _byte_ field_70[44];
 _Resources_ resourses;

 _byte_ field_B8[45];

 _byte_ field_E5;
 _byte_ human;

 _byte_ IsHumanAndWTF() {return CALL_1(_byte_, __thiscall, 0x4BAA40, this);}
 _byte_ IsHuman() {return CALL_1(_byte_, __thiscall, 0x4BAA60, this);}

 void MoveHeroToTownUp(_Town_* town) { CALL_2(void, __thiscall, 0x4B9C80, this, town);}

 //my
 inline _bool_ IsActive() {return (_bool_)(this == o_ActivePlayer);}
};


NOALIGN struct _Town_
{
 _int8_ id;
 _int8_ owner_id;
 _int8_ built_this_turn;
 _byte_ field_03;
 _int8_ type;
 _byte_ x;
 _byte_ y;
 _byte_ z;
 _byte_ boat_x;
 _byte_ boat_y;

 _word_ field_0A;

 _int_ up_hero_id;
 _int_ down_hero_id;

 _int8_ mag_level;
 _byte_ field_15;

 _word_ available_creatures[14];

 _byte_ fields_32[6];
 _dword_ field_38;
 _dword_ field_3C;
 _word_ field_40;
 _word_ field_42;

 _dword_ spells[5][6];
 _byte_ magic_hild[5];

 _byte_ fields_C1[7];

 _char_ name[12];

  int   _u8[3];         //* +D4 = 0

  _Army_ guards; //+E0 = охрана замка
  _Army_ guards0; //+118 = охрана замка

  _dword_   built_bits; //*B +150h = уже построенные здания (0400)
  _dword_   built_bits2;
  _dword_   bonus_bits;//*B +158h = бонус на существ, ресурсы и т.п., вызванный строениями
  _dword_   bonus_bits2;
  _dword_   available_bits;      //*B- +160h = маска доступных для строения строений
  _dword_   available_bits2;     

 // normal

 char* GetTypeName() {return CALL_1(char*, __thiscall, 0x5C1850, this);}
 _Army_* GetUpArmy() {return CALL_1(_Army_*, __thiscall, 0x5C1860, this);}
 void SwapHeroes() {CALL_1(void, __thiscall, 0x5BE850, this);}
 void MoveHeroDown() {CALL_1(void, __thiscall, 0x5BE790, this);}
    
    // Построено ли в городе здание.
    inline _bool32_ IsBuildingBuilt(_int32_ building_id, _bool32_ unk_unused)
    {
      return CALL_3(_bool32_, __thiscall, 0x4305A0, this, building_id, unk_unused);
    }
    
    
 // my

 //void MoveHeroUp() {o_GameMgr->GetPlayer(o_GameMgr->GetHero(this->down_hero_id)->owner_id)->MoveHeroToTownUp(this);}

};

NOALIGN struct _Spell_
{
 _int_ type; // -1 -enemy, 0 -area, +1 -friend
 _cstr_ wav_name;//+4
 _int_ animation_ix;//+8
 _dword_ flags; //+C
 _cstr_   name;            // +10h
 _cstr_   short_name;        // +14h
 _int_    level;           // +18h
 _dword_  school_flags; // +1Ch Air=1,Fire=2,Water=4,Earth=8
 _int_    mana_cost[4];         // +20h cost mana per skill level
 _int_    eff_power;       // +30h
 _int_    effect[4];       // +34h effect per skill level
 _int_    chance2get_var[9];// +44h chance per class
 _int_    ai_value[4];      // +68h 
 _cstr_   description[4];     // +78h
};


NOALIGN struct _CreatureAnim_ // size = 84 Cranim.txt
{ 
  _int16_ i[6];
  _float_ f[18];  
};

NOALIGN struct _BattleStack_ : _Struct_ // размер 0x548
{
 _byte_ already_attack; // +0 ?
 _byte_ field_01;  // +1 ?
 _byte_ field_02;  // +2 ?
 _byte_ field_03;  // +3 ?
 _byte_ field_04[4];  // +4 ?

 _int32_ all_stacks_count; // +8  //полное число стеков у игрока

 _byte_ field_0C[4];  // +12 0x0C 
 _dword_ field_10;  // +16 0x10  //=-1 после атаки и/или ответа(????)
 _byte_ field_14[8];  // +20 0x14  ?

 _int32_ aim_to_move_or_shoot; // 28 +0x1C  //позиция на поле боя (куда бежать/стрелять)
 _byte_ fireshield;  // +32 0x20 // огненный щит

 _byte_ field_21[3];  // +33 0x21  ?
 
 // Номер стека - хозяина данного клона (-1 - нет хозяина).
 _int32_ clone_owner_stack_ix; //+36 
 
 //_int32_ clone_index; // +40 0x28 //номер стэка клона этого
 _List_<_int32_>* clones; // Заменяем поле. Теперь это список клонов стека.
 
 _byte_ field_2C[4];  // +44 0x2C  ?
 _dword_ field_30;  // +48 0x30  ?

 _int32_ creature_id;    // +52 0x34 // тип монстра
 _int32_ hex_ix;   // +56 0x38  //позиция на поле боя 
 _int32_ def_group_ix; // +60 0x3C
 _int32_ def_frame_ix; // +64 0x40

 _dword_ field_44;  // +68 0x44 //(=1) сдвиг в сторону второй занятой клетки для монстра с двумя клетками
 _dword_ field_48;  // +72 0x48 ?

 _int32_ count_current; // +76 0x4C // число монстров
 _int32_ count_before_attack; // +80 +0x50 // число монстров до удара по ним в тек. атаку
 
 _dword_ field_54;  // +84 0x54 ?
 
 _int32_ lost_hp;  // +88 0x58 потери здоровья последнего монстра
 _int32_ army_slot_ix; // +92 0x5C номер слота of _Army_ (0...6), -1 - будет удален после битвы
 _int32_ count_at_start; // +96 0x60 число монстров в начале битвы
 
 _dword_ field_64;  // +100 0x64 ?
 
 _dword_ anim_value;  // +104 0x68 = cranim.f[15] при инициализации
 _int32_ full_hp;  // +108 0x6C полное здоровье (исп. как база для лечения)
 
 _bool32_ field_70;  // +112 0x70 ?

//////////////////////////////////////////////////////////////////////////////////
 _CreatureInfo_ creature;
//////////////////////////////////////////////////////////////////////////////////
 //_int_ town;  //+116 0x74
 //_int_ level;  //+120 0x78
 //_char_* sound_name;  //+124 0x7C
 //_char_* def_name;  //+128 0x80
 //_int_ flags;  //+132 0x84
 //_char_* name_single;  //+136 0x88
 //_char_* name_plural;  //+140 0x8C
 //_char_* specification_description;  //+144 0x90
 //_int_ cost_wood;  //+148 0x94
 //_int_ cost_mercury;  //+152 0x98
 //_int_ cost_ore;  //+156 0x9C
 //_int_ cost_sulfur;  //+160 0xA0
 //_int_ cost_crystal;  //+164 0xA4
 //_int_ cost_jems;  //+168 0xA8
 //_int_ cost_gold;  //+172 0xAC
 //_int_ fight_value;  //+176 0xB0
 //_int_ AI_value;  //+180 0xB4
 //_int_ growth;  //+184 0xB8
 //_int_ horde_growth;  //+188 0xBC
 //_int_ hit_points;  //+192 0xC0
 //_int_ speed;  //+196 0xC4   
 //_int_ attack;  //+200 0xC8
 //_int_ defence;  //+204 0xCC
 //_int_ damage_min;  //+208 0xD0
 //_int_ damage_max;  //+212 0xD4
 //_int_ shots;  //+216 0xD8
 //_int_ spells_count;  //+220 0xDC
 //_int_ advmap_low;  //+224 0xE0
 //_int_ advmap_high;  //+228 0xE4
///////////////////////////////////////////////////////////////////////////////

 _byte_ field_E8;  // +232 0xE8  ?

 _bool8_ is_1_killed; // +233 0xE9   =1, если умирал хоть один
 _bool8_ is_killed;  // +234 0xEA   =1, если был убит весь стэк

 _byte_ field_EB;  // +235 0xEB  ?

 _int32_ current_creatures_spell_id; // +236 0xEC  номер заклинания существа в тек раунде 0x50 Acid breath
 
 _int32_ field_F0;  // +240 0xF0  _byte_=1 перед атакой на него 441434
 
 _int32_ side;   // +244 0xF4  0 - attacker, 1 - defender
 
 _dword_ index_on_side;    // +248 F8 dd = номер стэка у стороны на поле боя
 _dword_ field_FC;  // +252 FC dd = ? что-то с магией
 _dword_ field_100;  // +256 100 dd 43DEA4
 _dword_ field_104;  // +260 104 dd 43DEAD
 _dword_ field_108;  // +264 108 dd
 _dword_ field_10C;  // +268 10C dd
 
 _CreatureAnim_ cr_anim; // +272 0x110 
 _Def_* def;    // +356 0x164 def монстра, загрузка: 43DA8E
 _Def_* def_shot;  // +360 0x168 def выстрела, загрузка: 43DA8E
 _dword_ field_16C;  // +364 0x16С ?
 _Wav_* wav_move;  // +368 0x170 звук перемещения (move), загрузка: 43DA8E
 _Wav_* wav_attack;  // +372 0x174 звук атаки (attk/wnce/shot), загрузка: 43DA8E
 _Wav_* wav_wnce;  // +376 0x178 звук 'колдует' (wnce), загрузка: 43DA8E
 _Wav_* wav_shot;  // +380 0x17C звук выстрела (shot), загрузка: 43DA8E
 _Wav_* wav_kill;  // +384 0x180 звук смерти (kill), загрузка: 43DA8E
 _Wav_* wav_defend;  // +388 0x184 звук обороны (wnce/dfnd), загрузка: 43DA8E
 _Wav_* wav_ext1;  // +392 0x188 звук дополнительный 1 (ext1), загрузка: 43DA8E
 _Wav_* wav_ext2;  // +396 0x18С звук дополнительный 2 (ext2), загрузка: 43DA8E
 
 _dword_ field_190;  // +400 0x190 ?
 
 _int32_ active_spells_count; // +404 0x194 dd = количество уже наложенных заклинаний
 _int32_ active_spell_duration[81]; // +408 0x198  есть заклинание (длительность) или нет
 _int32_ active_spells_power[81]; // +732 0x2DC (сила действия заклинания)
 
 _byte_ field_420[52]; // +1056 0x420 ?
 
 _int32_ retaliations; // +1108 0x454 // 441B17 (кол-во ответов на атаку 0= не отв. на атаку)// настройка для грифонов 46D6A0
 _int32_ bless_value; // +1112 0x458 Bless добавка к Max. Damage
 _int32_ curse_value; // +1116 0x45C Curse убавка к Min. Damage
 _int32_ field_560;  // +1120 0x460 ?
 _int32_ bloodlast_value;// +1124 0x464 Bloodlast добавка к Атаке с бонусами
 _int32_ precision_value;// +1128 0x468 Precision добавка к Атаке с бонусами
 
 _byte_ field_46C[32]; // +1132 0x46C ?
 
 _int32_ king_type;  // +1164 0x48C dd KING_123 тип (1=KING_1,2=KING_2,3=KING_3) //   исп для расчета Slayer. Бонус 8 к Атаке: 0x4421D2     
 _int32_ field_490;  // +1168 0x490 dd номер атакера по порядку??? уже атаковал??? (сбрасывается после первого удара)
 _int32_ counerstrike_retaliations; // +1172 0x494 dd кол. доп. ответов на атаку, добавленных Counerstrike заклом
 
 _byte_ field_498[40]; // +1176 0x498 ?
 
 _bool8_ blinded;   // +1216 0x4C0 db Blinded - снизить защиту (сбросить после?) при атаке на него (уст. перед ударом)
 _bool8_ paralized;   // +1217 0x4C1 db Paralized - снизить защиту (сбросить после?) при атаке на него (уст. перед ударом)
 _bool8_ forgetfulness;  // +1218 0x4C2 dd Forgetfulness - уровень (>2 - не может стрелять)
 
 _byte_ field_4C3[25];  // +1219 0x4C3 ?
 
 _int32_ defend_bonus;  // +1244 0x4DC dd = величина бонуса при выборе защиты
 _int32_ faerie_dragon_spell;// +1248 0x4E0 dd заклинание для сказ дракона

 _byte_ field_4E4[100];

                     // +4EC dd 44152A
                     // +4F1 db 43DF88

                     // +514 dd
                     // +518 dd -> dd first \ adjusted stacks pointers
                     // +51C dd -> dd last  /

                     // +524 dd
                     // +528 dd -> dd first \ adjusted to wich stacks pointers
                     // +52C dd -> dd last  /


 // normal
 inline _bool_ CanShoot(_BattleStack_* aim = NULL) {return CALL_2(_bool_, __thiscall, 0x442610, this, aim);}
 inline void InitDefsAndWavs() {CALL_1(void, __thiscall, 0x43D710, this);}
 inline void Construct(int creature_id, int count, _Hero_* hero_owner, int side, int index_on_side/*0 - 21*/, int position_hex_ix,  int army_slot_ix)
  {CALL_8(void, __thiscall, 0x43D5D0, this, creature_id, count, hero_owner, side, index_on_side, position_hex_ix, army_slot_ix);}
 
 
 
 // Подсчёт добавок к наносимому урону (возвращается новый урон).
 // Enemy - указатель на атакуемый стек.
 // BaseDamage - начальный урон.
 // IsShot - является ли атака выстрелом (TRUE - является).
 // Virtual - является ли подсчёт производимым ИИ (TRUE - является).
 // WayLength - пройденный путь до цели (для расчёта кавалерийского бонуса).
 // out_FireshieldDamage - указатель на переменную, в которую в результате работы функции запишется урон от огненного щита (0 - не подсчитывать).
 inline _int32_ Calc_Damage_Bonuses(_BattleStack_* Enemy, _int32_ BaseDamage, _bool8_ IsShot, _bool8_ Virtual, _int32_ WayLength, _int32_* out_FireshieldDamage)
 {
   return CALL_7(_int32_, __thiscall, 0x443C60, this, Enemy, BaseDamage, IsShot, Virtual, WayLength, out_FireshieldDamage);
 }
 
 
 
 
 // Отрисовка выстрела стека по противнику и полёта снаряда.
 inline void Draw_Shot(_BattleStack_* Enemy)
 {
   CALL_2(void, __thiscall, 0x43EFE0, this, Enemy);
 }
 
 
 // Получение второго гекса, занимаемого стеком (если стек 1-клеточный, первого).
 inline _int32_ Get_SecondHex_ix()
 {
   return CALL_1(_int32_, __thiscall, 0x4463C0, this);
 }
 
 
 // Уничтожение стека.
 // dead_itself - умер ли стек сам собой (например, как клон при гибели хозяина или окончании длительности заклинания).
 inline void Die(_bool8_ dead_itself)
 {
   CALL_2(void, __thiscall, 0x443E40, this, dead_itself);
 }
 
 
 
 
 
 // User
 
 // Получение номера стрелковой башни стека (-1 - стек не является стрелковой башней).
 inline _int32_ Get_ArrowTowerNum()
 {
   // Выбираем номер стрелковой башни в зависимости от её позиции (стандартный способ игры).
   switch (this->hex_ix)
   {
     case 254:
       return 0;
     case 251:
       return 1;
     case 255:
       return 2;
     default:
       return -1;
   }
 }
 
};




// Структура стрелковой башни в бою.
NOALIGN struct _ArrowTower_
{
  _int32_ CreatureType; // +0h; Тип существа башни
  _Def_* Def; // +4h; Загруженный def существа башни
  _Def_* BulletDef; // +8h; Загруженный def снаряда башни
  _int32_ X_Position; // +Ch; Позиция изображения центра существа башни по горизонтали в пикселях
  _int32_ Y_Position; // +10h; Позиция изображения низа существа башни по вертикали в пикселях
  _int32_ Orientation; // +14h; Направление (0 - вправо, 1 - влево)
  _int32_ AnimSectionNum; // +18h; Номер секции кадров анимации башни
  _int32_ AnimFrameNum; // +1Ch; Текущий кадр секции анимации башни
  _int32_ StackNum; // +20h; Номер стека башни (сторона-владелец - всегда 1)
};






// Структура боевого элемента замка (массив из 8-ми элементов по 0x63BE60).
NOALIGN struct _CastleElement_
{
  _int16_ GexNum; // +0h; Позиция элемента (используется при его выборе как цели атаки)
  _int16_ SecondGex_Row; // +2h; Ряд второй позиции элемента (-1 - нет)
  _int16_ X_Position; // +4h; Горизонтальная позиция элемента в пикселях
  _int16_ Y_Position; // +6h; Вертикальная позиция элемента в пикселях
  _int32_ CastleDrawingPartNum; // +8h; Номер элемента замка среди всех отрисовывающихся частей замка
};











NOALIGN struct _BattleHex_
{
 //_byte_ field_0[112]; //?
 _int16_ X_Position; // +0h; Позиция центра гекса по горизонтали в пикселях
 _int16_ Y_Position; // +2h; Позиция низа гекса по вертикали в пикселях
 _int16_ Left; // +4h; X-координата левой границы гекса
 _int16_ Top; // +6h; Y-координата верхней границы гекса
 _int16_ Right; // +8h; X-координата правой границы гекса
 _int16_ Bottom; // +Ah; Y-координата нижней границы гекса
 _byte_ field_C[4]; //?
 _int32_ Flags; // +10h; Флаги гекса (1 - есть ли препятствие)
 _int_ ObstacleNum; // +14h; Номер препятствия, находящегося на стеке (-1 - нет)
 _byte_ bstack_side;
 _byte_ bstack_index;
 _byte_ field_1A[86]; //?
 
 
 // normal
 _BattleStack_* GetCreature() {return CALL_1(_BattleStack_*, __thiscall, 0x4E7230, this);}
};


// Структура боевой анимации.
NOALIGN struct _BattleAnim_
{
  _cstr_ DefName; // Имя def`а анимации
  _cstr_ TouchEffect_Name; // Имя тактильного эффекта анимации (для его воспроизведения на нестандартных элементах управления с помощью IFC20.dll)
  _dword_ Properties; // Свойства боевой анимации (по несколько битов)
  // ***
  // Начало описания свойств анимации
  // ***
    // (сдвиг - размер в битах - значение) 
    // 0 - 4 - Позиция отображения (0 - от земли на стеке или гексе, 1 - посередине гекса или стека, 2 - выше стека, 3 - перед стеком, 4 - с нижней и левой границы гекса, -1 - нет позиции)
    // 4 - 4 - Не используется
    // 8 - 1 - Прозрачность (0 - нет, 1 - есть)
    // 9 - 23 - Не используется
  // ***
  // Конец описания свойств анимации
  // ***
};


NOALIGN struct _BattleMgr_ : _Struct_ 
{
 _byte_ field_0[452]; // + 0 ?
 _BattleHex_ hex[187]; // + 0x1c4 187=17*11
 
 
 _byte_ field_5394[44]; //?
 
 // Тип территории (-1 - нет, 0 - побережье, 1 - равнина магов, 2 - проклятая земля и т. д.)
 _int32_ spec_terr_type;
 
 _byte_ field_53C4[8]; //?

 _Hero_* hero[2]; // + 21452 // 0 - attacker, 1 - defender
 _byte_ field_53D4[212]; // + 21460dя
 _int32_ owner_id[2]; // + 21672d // 0 - attacker, 1 - defender
 _byte_ field_54B0[12];  // + 21680
 _int32_ stacks_count[2];//+0x54BC  // 0 - attacker, 1 - defender
 _Army_* army[2]; // + 21700 // 0 - attacker, 1 - defender
 //_BattleStack_ stack[42]; //+ 21708
 _BattleStack_ stack[2][21]; //+ 21708

 _byte_ field_1329C[28]; // + 0x1329C
 _int32_ unk_side; // +78520 0x132B8
 _int32_ current_stack_ix; // +78524 0x132BC
 _int32_ current_side; // +78528 0x132C0

 //_byte_ field_132C4[56]; // + 0x132C4
 _byte_ field_132C4[36]; // + 0x132C4
 _Def_* current_spell_def; // + 0x132E8
 _int_  current_spell_id; // + 0x132EC
 _dword_ field_132F0; // + 0x132F0
 _int32_ town_fort_type; // + 0x132F4
 _dword_ field_132F8; // + 0x132F8

 _Dlg_* dlg;    // + 0x132FC
 
 _byte_ f13300[3564]; // +13300h

 // normal
 //inline _Def_* LoadSpellAnimation(int spell_anim_ix)
 inline int GetHexIxAtXY(int x, int y) {return CALL_2(int, __stdcall, 0x464380, x - dlg->x, y - dlg->y);}
 inline void CastSpell(int spell_id, int hex_ix, int cast_type_012, int hex2_ix /*для телепорта и жертвы*/, int skill_level, int spell_power)
  {CALL_7(void, __thiscall, 0x5A0140, 
    this, spell_id, hex_ix, cast_type_012, hex2_ix, skill_level, spell_power);}
 
 
 // Проверка необходимости отрисовки битвы.
 // При необходимости отрисовки возвращает FALSE, иначе TRUE.
 inline _bool8_ ShouldNotRenderBattle()
 {
   return CALL_1(_bool8_, __thiscall, 0x46A080, this);
 }
 
 
 
 // Сброс полей необходимости перерисовки.
 inline void ClearRedrawFields()
 {
   return CALL_1(void, __thiscall, 0x493290, this);
 }
 
 
 // Указание необходимости перерисовки при следующей отрисовке для стека - стрелковой башни.
 inline void SetArrowTowerToRedraw(_BattleStack_* Stack)
 {
   return CALL_2(void, __thiscall, 0x46A040, this, Stack);
 }
 
 
 
 // Определение границ перерисовки для кадра def`а.
 inline void SetSpecRedrawBorders(_Def_* Def, _int_ DefGroupNum, _int_ FrameNum, _int_ X_Pos, _int_ Y_Pos, _RedrawBorders_* out_Borders, _bool_ Reflected, _bool_ NeedAddToGlobalBorders)
 {
   CALL_9(void, __thiscall, 0x495AD0, this, Def, DefGroupNum, FrameNum, X_Pos, Y_Pos, out_Borders, Reflected, NeedAddToGlobalBorders);
 }
 
 
 
 
 
 // Вычисление границ необходимости обновления экрана на основе полей необходимости перерисовки боевых элементов.
 inline void SetRedrawBorders()
 {
   CALL_1(void, __thiscall, 0x495770, this);
 }
 
 
 // Отрисовка поля боя.
 // Flip - необходимость обновления экрана.
 // SetBattleRedraws - необходимость настройки границ обновления экрана.
 // UseBattleRedraws - необходимость использования границ обновления экрана (при SetBattleRedraws = TRUE игнорируется, считаясь TRUE).
 // WaitingTime - время, на ожидание которого надо настроить следующую отрисовку (игнорируется при Wait = FALSE).
 // RedrawBackground - необходимость перерисовки заднего плана (и стёрки старого изображения).
 // Wait - необходимость ожидания. В случае FALSE время ожидания для следующей отрисовки не меняется.
 inline void RedrawBattlefield(_bool8_ Flip, _bool8_ SetBattleRedraws, _bool8_ UseBattleRedraws, _int_ WaitingTime, _bool8_ RedrawBackground, _bool8_ Wait)
 {
   return CALL_7(void, __thiscall, 0x493FC0, this, Flip, SetBattleRedraws, UseBattleRedraws, WaitingTime, RedrawBackground, Wait);
 }
 
 
 // Обновление всего поля боя.
 inline void FlipBattlefield()
 {
   CALL_1(void, __fastcall, 0x493300, this);
 }
 
 
 
 
 // Инициализация переменных, управляющих временем случайных анимаций.
 inline void InitRandAnimsTimes()
 {
   CALL_1(void, __thiscall, 0x479860, this);
 }
 
 
 // Проигрывание одного кадра анимации ожидания поля боя.
 // Это анимация флагов героев, анимация самих героев, случайная анимация существ, мигающая рамка вокруг стеков.
 inline void PlayWaitAnim()
 {
   CALL_1(void, __thiscall, 0x495C50, this);
 }
 
 
 // Проигрывание одного кадра анимации ожидания поля боя только в случае наступления времени.
 inline void PlayWaitAnimOnce()
 {
   CALL_1(void, __thiscall, 0x473970, this);
 }
 
 
 // Нанесение урона элементу замка по его номеру.
 inline void DamageCastleElement(_int_ CastleElementNum, _int_ Damage)
 {
   CALL_3(void, __thiscall, 0x465570, this, CastleElementNum, Damage);
 }
 
 
 
 // Удаление стека с гекса, на котором он стоит.
 inline void RemoveStackFromGexes(_BattleStack_* Stack)
 {
   CALL_2(void, __thiscall, 0x468310, this, Stack);
 }
 
 
 // Постановка стека на гекс.
 inline void PutStackToGex(_BattleStack_* Stack, _int_ GexNum)
 {
   CALL_3(void, __thiscall, 0x4683A0, this, Stack, GexNum);
 }
 
 // Удаление препятствия.
 inline void RemoveObstacle(_int_ ObstacleNum)
 {
   CALL_2(void, __thiscall, 0x466710, this, ObstacleNum);
 }
 
 
 
 
 // my
 inline _BattleStack_* GetCurrentStack()
 {
  return &(stack[current_side][current_stack_ix]);
 }

 inline _BattleStack_* GetCreatureAtXY(int x, int y)
 {
  int hex_ix = GetHexIxAtXY(x, y);
  if (hex_ix != ID_NONE) 
   return hex[hex_ix].GetCreature();
  else
   return NULL;
 }
 
 
 
 
  // Установка поля перерисовки для стека.
 inline void Set_Stack_Redrawable(_BattleStack_* Stack)
 {
   // Специальная функция для стрелковой башни.
   if (Stack->creature_id == CID_ARROW_TOWER)
   {
     this->SetArrowTowerToRedraw(Stack);
   }
   // Специальное поле для стеков.
   else
   {
     *(_bool8_*)((_dword_)this + 81920 + Stack->side*20 + Stack->index_on_side) = TRUE;
   }
 }
 
 
 
 
 
 
 // Добавление области к границам перерисовки. Возврат необходимости отрисовки изображения (по настройкам).
 // Возвращение необходимости перерисовки.
 inline _bool_ AddUpdateArea(_int_ X_Pos, _int_ Y_Pos, _int_ Width, _int_ Height)
 {
   
   // Границы перерисовки.
   _RedrawBorders_ Brd;
   
   
   if (*(_bool8_*)((_ptr_)this + 81196) || *(_bool32_*)((_ptr_)this + 81200))
   {
     // Левая граница перерисовки.
     Brd.Left = max(X_Pos, BattleRedraw_Borders.Left);
     // Верхняя граница перерисовки.
     Brd.High = max(Y_Pos, BattleRedraw_Borders.High);
     // Правая граница перерисовки.
     Brd.Right = min(X_Pos + Width - 1, BattleRedraw_Borders.Right);
     // Нижняя граница перерисовки.
     Brd.Low = min(Y_Pos + Height - 1, BattleRedraw_Borders.Low);
     
     if (*(_bool8_*)((_ptr_)this + 81196))
     {
       // Настройка глобальных границ перерисовки.
       (*(_int_*)((_ptr_)this + 81208)) = min(*(_int_*)((_ptr_)this + 81208), Brd.Left);
       (*(_int_*)((_ptr_)this + 81212)) = min(*(_int_*)((_ptr_)this + 81212), Brd.High);
       (*(_int_*)((_ptr_)this + 81216)) = max(*(_int_*)((_ptr_)this + 81216), Brd.Right);
       (*(_int_*)((_ptr_)this + 81220)) = max(*(_int_*)((_ptr_)this + 81220), Brd.Low);
     }
     
     if (*(_bool32_*)((_ptr_)this + 81204)) return FALSE;
   }
   
   
   // Возвращаем необходимость отрисовки (если хоть частично изображение попало в границы перерисовки).
   if (*(_bool32_*)((_ptr_)this + 81200)
       && (Brd.Left > ((_RedrawBorders_*)((_ptr_)this + 81208))->Right || Brd.Right < ((_RedrawBorders_*)((_ptr_)this + 81208))->Left
           || Brd.High > ((_RedrawBorders_*)((_ptr_)this + 81208))->Low || Brd.Low < ((_RedrawBorders_*)((_ptr_)this + 81208))->High))
   {
     return FALSE;
   }
   else
   {
     return TRUE;
   }
 }
 
 
 // Добавление области к границым перерисовки способом стека.
 // Возвращение необходимости перерисовки.
 inline _bool_ AddSpecUpdateArea(_Def_* Def, _int_ DefGroupNum, _int_ FrameNum, _int_ X_Pos, _int_ Y_Pos, _bool_ Reflected)
 {
   // Границы перерисовки.
   _RedrawBorders_ Brd;
   
   if (*(_bool8_*)((_ptr_)this + 81196) || *(_bool32_*)((_ptr_)this + 81200))
   {
     this->SetSpecRedrawBorders(Def, DefGroupNum, FrameNum, X_Pos, Y_Pos, &Brd, Reflected, *(_bool8_*)((_ptr_)this + 81196));
     if (*(_bool32_*)((_ptr_)this + 81204)) return FALSE;
   }
   // Возвращаем необходимость отрисовки (если хоть частично изображение попало в границы перерисовки).
   if (*(_bool32_*)((_ptr_)this + 81200)
       && (Brd.Left > ((_RedrawBorders_*)((_ptr_)this + 81208))->Right || Brd.Right < ((_RedrawBorders_*)((_ptr_)this + 81208))->Left
           || Brd.High > ((_RedrawBorders_*)((_ptr_)this + 81208))->Low || Brd.Low < ((_RedrawBorders_*)((_ptr_)this + 81208))->High))
   {
     return FALSE;
   }
   else
   {
     return TRUE;
   }
 }
 
 
 
 
};

NOALIGN struct _TownStartInfo_ //size = 0x88
{
 _int_ id; //+0
 _byte_ owner_id;//+4
 _byte_ field_5[19];//+5
 _bool8_ has_fort; //+0x18
 _byte_ field_19[3];//+0x19
 _Army_ army;//+0x1C
 _byte_ field_54[20];//+0x54
 _dword_ type;//+0x68
 _byte_ field_6C[28];//+0x54
};

NOALIGN struct _ArmyStartInfo_
{
 _int32_ type[7];
 _int16_ count[7];
};

NOALIGN struct _HeroStartInfo_ //size = 0x334 bytes
{
 _int8_ owner_id;   //! = хозяин (цвет) ff - ничей
 _byte_ _u1[3];
 _dword_ id;  // = номер подтипа (конкретный герой)
 _dword_ id_wtf;      // = id
 _bool8_ has_name;    // = 1-есть имя
 _char_ name[13];//! = имя,0
 _byte_ has_exp;    // = 1-есть опыт
 _byte_ _u2;
 _int_  exp;     //   +1c  dd   = опыт
 _bool8_   has_picture;    //   +20  db   = 1-есть картинка
 _byte_    picture_id;     //   +21  db   = номер картинки
 _bool8_   has_second_skills;   //   +22  db   = 1-есть 2-е скилы
 _byte_ _u3;
 _dword_ second_skills_count;   //!   +24  dd   = кол. вторых скилов
 _byte_ second_skill[8];//  +28  dd*8 = номера вторых скилов
 _byte_ second_skill_level[8];//   +30  db*8 = уровни вторых скилов
 _bool8_ has_army;    //   +38  db   = 1-есть существа
 _byte_ _u4[3];

 _ArmyStartInfo_ army; //+3c
 _bool8_ army_is_group;   //   +66  db   = группа/разброс

 _bool8_ has_arts;    //   +67  db   = 1-есть артифакты
 _Artifact_ doll_art[19];//+68  dd*2*13 = артифакты dd-номер,dd-(ff) +e8 -книга(3,ff)
 _Artifact_ backpack_art[64];//+100 dd*2*40 = арт в рюкзаке dd-номер, dd-(ff)
 _byte_ backpack_arts_count;   //   +300 db   = число артифактов в рюкзаке
 _dword_ xyz;   //   +301 2*dw = нач. позиция на карте
 _byte_ run_radius;     //!   +305 db   = радиус обегания
 _bool8_ has_biography;    //   +306 db   = 1-есть биография
 _byte_ _u6;   //+307
 _HStringF_ biography;      //   +308 dd   = 1-выделена память под биографию
 _int8_ sex;     //   +318 dd   = 0-м,1-ж,ff-умолчание
 _byte_ _u9[3];
 _bool8_ has_spells;   //   +31c db   = 1-есть заклинания
 _byte_ _u10[3];
 _byte_ spell[10];//  +320 db*a = заклинания
 _word_ _u11;
 _byte_ has_primary_skills;   //   +32c db   = 1-есть первичные умения
 _byte_ primary_skill[4];//  +32d db*4 = 4-ре первичных умения
 _byte_ _u12[3];
};

#define GAME_HEROES_AVAILABILITY_OFFSET (*(_ptr_*)0x412F1C)
#define GAME_PLAYERS_AVAILABILITY_OFFSET (*(_ptr_*)0x4868FB)

#define GAME_HEROES_OFFSET (*(_ptr_*)0x41C6B5)






// ******************************




// Список героев.
NOALIGN struct _HeroesList_: public _List_<_Hero_>
{
  // Нет дополнительных полей, только методы.
  
  // Удаление списка героев.
  inline void Delete()
  {
    CALL_1(void, __thiscall, 0x45F830, this);
  }
};




// Структура битовой маски проходимости и триггеров шаблона.
NOALIGN struct _TmplPosMsk_
{
  // Первые 32 бита.
  _dword_ bits0; // +0
  // Последние 32 бита (используются только первые 16).
  _dword_ bits32; // +4
  
  
  // Установка всех битов маски.
  void SetAll(_dword_ value)
  {
    CALL_2(void, __thiscall, 0x4E6770, this, value);
  }
  
  
  // Установка указанного бита маски.
  void SetBit(_int_ bit_ix, _bool8_ value)
  {
    CALL_3(void, __thiscall, 0x506DD0, this, bit_ix, value);
  }
  
  
  // Получение указанного бита маски.
  _bool8_ GetBit(_int_ bit_ix)
  {
    return CALL_2(_bool8_, __thiscall, 0x506E30, this, bit_ix);
  }
  
};





// Структура битовой маски доступности территорий шаблона.
NOALIGN struct _TmplTerrMsk_
{
  // Первые 32 бита.
  _dword_ bits0; // +0
  
  
  // Установка всех битов маски.
  void SetAll(_dword_ value)
  {
    CALL_2(void, __thiscall, 0x506ED0, this, value);
  }
  
  
  // Установка указанного бита маски.
  void SetBit(_int_ bit_ix, _bool8_ value)
  {
    return CALL_3(void, __thiscall, 0x506E70, this, bit_ix, value);
  }
  
  
  
  // user
  
  // Получение указанного бита маски.
  _bool8_ GetBit(_int_ bit_ix)
  {
    return (((1 << (bit_ix & 0x1F)) & *(&this->bits0 + (bit_ix >> 5))) != 0);
  }
  
};





// Шаблон объекта на карте.
// Отвечает за тип объекта и всевозможные свойства, которые могут быть разными у одного тппа - проходимость и т. п.
NOALIGN struct _Template_
{
  // Имя def`а шаблона.
  _HStringF_ DefName; // + 0
  // Размер шаблона по-горизонтали (в клетках карты).
  _byte_ Size_x; // +16
  // Размер шаблона по-вертикали (в клетках карты).
  _byte_ Size_y; // +17
  _byte_ f12[10]; // +18
  // Битовая маска проходимых клеток (6*6, 2 бита в каждом байте и 2 байта полностью не используются).
  _TmplPosMsk_ ImpassBitMask; // +28
  _byte_ f24[8]; // +36
  // Битовая маска активируемых клеток (6*6, 2 бита в каждом байте и 2 байта полностью не используются).
  _TmplPosMsk_ TriggerBitMask; // +44
  _TmplTerrMsk_ terrs; // +52
  // Тип объекта шаблона.
  _int_ ObjType; // +56
  // Подтип объекта шабона.
  _int_ ObjSubtype; // +60
  _byte_ f40; // +64
  _byte_ f41[3]; // +65
};



// Объект на карте приключений.
NOALIGN struct _Object_
{
  _byte_ f0[4]; // +0h
  // X-координата положения на карте
  _byte_ x; // +4h
  // Y-координата положения на карте
  _byte_ y; // +5h
  // Z-координата положения на карте
  _byte_ z; // +6h
  // Результат выравнивания, не используется.
  _byte_ dummy_f7[1]; // +7h
  // Номер шаблона объекта.
  _int16_ template_id; // +8h
  _byte_ dummy_fA[2]; // +Ah
};



// Объект - лагерь героя.
NOALIGN struct _PlaceHolder_
{
  // Адрес общей структуры объекта лагеря.
  _Object_* object; // +0h
  // Номер игрока - хозяина лагеря.
  _byte_ player_ix; // +4h
  // Результат выравнивания, не используется.
  _byte_ dummy_f5[3]; // +5h
  // Номер героя лагеря (-1 - произвольный).
  _int_ hero_id; // +8h
  // Условная сила героя в лагере для произвольного героя.
  _byte_ hero_power; // +Ch
  // Результат выравнивания, не используется.
  _byte_ dummy_fD[3]; // +Dh
};





// Карта игры (размер 3932 или 3936 - неизвестно, относится ли последнее dd к карте или просто к GameMgr).
NOALIGN struct _GameMap_
{
  // Список шаблонов объектов карты.
  _List_<_Template_> Templates; // +0
  // Список объектов на карте.
  _List_<_Object_> Objects; // +16
  
  _byte_ f20[128]; // +32
  
  // Список лагерей героя на карте.
  _List_<_PlaceHolder_> PlaceHolders; // +160
  
  _byte_ fB0[32]; // +176
  
  // Адрес 3-мерного массива [L][Y][X], хранящего свойства объекта каждой клетки карты.
  _MapItem_* items; // +208
  // Размер карты (по-горизонтали и по-вертикали).
  _int_ size; // +212
  // Есть ли у карты подземный уровень.
  _bool8_ has_underground; // +216
  
  _byte_ fD9[3715]; // +217
  
  
  //normal
  // Получение объекта карты по координатам (из массива items).
  inline _MapItem_* GetItem(_int_ x, _int_ y, _int_ z)
  {
     return CALL_4(_MapItem_*, __thiscall, 0x4086D0, this, x, y, z);
  }
};


// Заголовочная информация карты. Размер: 720 байт.
NOALIGN struct _MapHeader_
{
  // +0.
  _byte_ f0[772];
};




NOALIGN struct _GameMgr_: _Struct_
{
 _dword_ field_0;//+0 ???
 _byte_ disabled_shrines[70]; //+4 //??? //какие святыни еще можно поставить на карту (0)
 _byte_ disabled_spells[70];//+0x4A //разрешено закл (0) на карте или запрещено (1)
 LPCRITICAL_SECTION cs_bink;//+0x90 //CriticalSection Object для запуска/остановки видео BINKW32.DLL
 _List_<_TownStartInfo_> town_start_info;//+0x94
 _HeroStartInfo_ hero_start_info[156];//+0xA4
 _dword_ field_1F454;//+0x1F454
 _bool8_ is_cheater;//+0x1F458
 
 _byte_ f1F459[1]; // +1F459h
 
 // Номер сценария в кампании (с 1).
 _byte_ campaing_scenario_ix; // +1F45Ah
 
 _byte_ f1F45B[1]; // +1F45Bh
 
 // Номер кампании (определяющий её карту и пр.)
 _dword_ campaing_ix;  // +1F45Ch
 
 
 _byte_ f1F460[52]; // +1F460h
 
 
 // Списки сохранённых героев в кампании для перехода в следующие сценарии.
 _List_<_HeroesList_> saved_heroes_lists; // +1F494h
 
 
 _byte_ f1F4A4[410]; // +1F4A4h
 
 
 // Номер текущего дня (1-7).
 _word_ curr_day_ix; // +1F63Eh
 
 // Номер текущей недели (1-4).
 _word_ curr_week_ix; // +1F640h
 
 // Номер текущего месяца.
 _word_ curr_month_ix; // +1F642h
 
 
 _byte_ f1F644[84]; // +1F644h
 
 
 // Версия карты (0 - RoE).
 _dword_ map_version; // +1F698h
 
 _byte_ f1F69C[1]; // +1F69Ch
 
 // Является ли текущая игра обучением.
 _bool8_ is_tutorial; // +1F69Dh
 
 _byte_ f1F69E[59]; // +1F69Eh
 
 // Название файла карты.
 char map_name[248]; // +1F6D9h
 
 _byte_ f1F7D1[3]; // +1F6D1h
 
 // Путь к файлу карты.
 char map_path[100]; // +1F7D4h
 
 
  _byte_ f1F838[52]; // +1F838h
  
  // Заголовочная информация карты игры.
  _MapHeader_ map_header; // +1F86Ch
  
  // Карта игры.
  _GameMap_ Map; // +1FB70h
 
  _byte_ f20ACC[2900]; // +20ACCh
  
  // Герои (перенесено).
  _Hero_ hero[156]; // +21620h
  
  // Положения героев (перенесено).
  _byte_ heroes_aval_abyte4DF18[156]; // +4DF18h
  
  // Доступности героев игрокам (перенесено).
  _dword_ hero_player_aval_f4DFBD[156]; // +4DFBDh
  
  _byte_ f4E22D[144]; // +4E22Dh
  
  _byte_ arts_aval_abyte4E2BD[144]; // +4E2BDh
  
  _byte_ f4E34D[1164]; // +4E34Dh
  
 
 inline _Player_* GetMe() {return CALL_1(_Player_*, __thiscall, 0x4CE670, this);}
 inline _int_  GetMeID() {return CALL_1(_int_, __thiscall, 0x4CE6E0, this);}
 inline char*  GetPlayerName(_int_ player_id) {return (char*)( ((_ptr_)this) + 0x20AD0 + player_id*0x168 + 0xCC );}

 inline _int_  FindTownIdByXYZ(_int_ x, _int_ y, _int_ z) {return CALL_4(_int_, __thiscall, 0x4BB530,this, x, y, z);}

 inline _Hero_*  GetHero(_int_ hero_id) {return ((_Hero_ *)(((_ptr_)this) + GAME_HEROES_OFFSET /*0x21620*/ + 1170 * hero_id));}
 inline _Player_* GetPlayer(_int_ player_id) {return ((_Player_ *)(((_ptr_)this) + 0x20AD0 + 360 * player_id));}
 inline _Town_*  GetTown(_int_ town_id) {return ((_Town_*)(*(_ptr_*)(((_ptr_)this) + 0x21614) + 360 * town_id));}
 inline _int_  GetTownsCount() {return ((_int_)((*(_dword_*)(((_ptr_)this) + 0x21618) - *(_dword_*)(((_ptr_)this) + 0x21614)) / 360));}
 inline _GlobalEvent_* GetGlobalEvent(_int_ index) {return ((_GlobalEvent_*)(*(_ptr_*)(((_ptr_)this) + 0x1fbf4) + 52 * index));}
 inline _int_  GetGlobalEventsCount() {return ((_int_)((*(_dword_*)(((_ptr_)this) + 0x1fbf8) - *(_dword_*)(((_ptr_)this) + 0x1fbf4)) / 52));}
 inline _int_  GetMapType() {return ( *(_dword_*)(((_ptr_)this) + 0x1f698) );}
 inline char*  GetScenarioName() {return *(char**)((_ptr_)this +  0x1F86C + 0x2D4);}
 inline _word_  GetDay() {return *(_word_*)((_ptr_)this + 0x01F63E);}
 inline _word_  GetWeek() {return *(_word_*)((_ptr_)this + 0x01F640);}
 inline _word_  GetMonth() {return *(_word_*)((_ptr_)this + 0x01F642);}
 inline _int_*  GetMerchantsArts() {return (_int_*)((_ptr_)this + 0x01F664);}


 inline _dword_  GetMapWidth() {return *(_dword_*)((_ptr_)this + 0x1FC44);}
 inline _byte_  GetMapDepth() {return *(_byte_*)((_ptr_)this + 0x1FC48);}

 inline _bool_  IsPlayerIngame(_int_ player_id) { return (_bool_)(1 - *(_byte_*)(((_ptr_)this) + 0x1F636 + player_id)); }
 inline _bool_  IsPlayerOutgame(_int_ player_id) { return (_bool_)(*(_byte_*)(((_ptr_)this) + 0x1F636 + player_id)); }

 inline void ReplayOpponentTurn() {CALL_1(void, __thiscall, 0x49D410, this);}
 inline void ShowScenarioInfo() {CALL_1(void, __thiscall, 0x513950, this);}

 inline _byte_ PlayerIsInGameHuman(_int_ player_id) {return CALL_2(_byte_, __thiscall, 0x4CE630, this, player_id);}

 inline void  ShowFullCreatureInfoDlg(_Army_* army, _int_ index, _Hero_* hero, _int_ e0, _int_ x, _int_ y, _int8_ can_fired, _int8_ right_click)
  { CALL_9(void, __thiscall, 0x4C6910, this, army, index, hero, e0, x, y, can_fired, right_click); }

 // my
 inline void ShowFullCreatureInfoDlg(_Army_* army, _int_ index, _Hero_* hero, _int_ x, _int_ y, _bool8_ right_click)
 {

  if (hero)
   CALL_9(void, __thiscall, 0x4C6910, this, army, index, hero, 0, x, y, hero->army.GetStacksCount() > 1, right_click);
  else
   CALL_9(void, __thiscall, 0x4C6910, this, army, index, hero, 0, x, y, TRUE, right_click);
 }
 
  
  
  // user
  
  // Получение лодки на координатах.
  _Struct_* GetBoatAtCoords(_int_ x, _int_ y, _int_ z)
  {
    // Проходим по всем существующим лодкам.
    if (this->Field<_List_<_byte_>>(320440).Data)
    {
      for (_ptr_ i = (_ptr_)this->Field<_List_<_byte_>>(320440).Data; i < (_ptr_)this->Field<_List_<_byte_>>(320440).EndData; i += 40)
      {
        // Лодка существует и не имеет героя.
        if (((_Struct_*)i)->Field<_bool8_>(24) && !((_Struct_*)i)->Field<_bool8_>(36))
        {
           // Лодка - на текущих координатах.
           if (((_Struct_*)i)->Field<_int16_>(0) == x && ((_Struct_*)i)->Field<_int16_>(2) == y && ((_Struct_*)i)->Field<_int16_>(4) == z)
           {
             return (_Struct_*)i;
           }
        }
      }
    }
    
    // Не нашли.
    return NULL;
  }
  
  
  // Посадка героя в лодку на том месте, где он стоит (герой должен быть на карте и не в лодке, если лодки там же нет, она создаётся, если есть - она должна там стоять с начала игры).
  _bool32_ HeroPlaceToBoat(_Hero_* hero, _int_ boat_subtype)
  {
    
    // Лодка.
    _Struct_* boat;
    
    // Создаём лодку.
    _int_ boat_ix = CALL_7(_int_, __thiscall, 0x4BAF10, this, hero->x, hero->y, hero->z, hero->owner_id, FALSE, boat_subtype);
    
    // Не удалсь создать лодку.
    if (boat_ix < 0)
    {
      return FALSE;
    }
    // Сгенерировали лодку.
    else
    {
      boat = (_Struct_*)(this->Field<_List_<_byte_>>(320440).Data + 40*boat_ix);
    }
    
    
    
    // Сажаем героя в лодку.
    
    
    // Убираем лодку из информации клетки.
    CALL_1(void, __thiscall, 0x4D7950, boat);
    
    // Настраиваем героя.
    hero->temp_mod_flags |= 0x40000;
    hero->field_112 = -1;
    *(_int32_*)&hero->d_morale = -1;
    
    // Пересчитываем ходьбу героя в водную (new_mp = old_mp*new_max/old_max).
    if (!(hero->temp_mod_flags & 0x1000000))
    {
      _int_ new_max = CALL_2(_int32_, __thiscall, 0x4E4C00, hero, TRUE);
      hero->movement_points = hero->movement_points*new_max/hero->movement_points_max;
      hero->movement_points_max = new_max;
    }
    
    // Настраиваем лодку.
    boat->Field<_int32_>(32) = hero->id;
    boat->Field<_bool8_>(36) = TRUE;
    boat->Field<_int8_>(28) = hero->owner_id;
    
    return TRUE;
  }
 
 
};



// ******************************






// *************************************


#define o_GetTime() CALL_0(_dword_, __cdecl, 0x4F8970)

typedef void (__thiscall *_func_MouseMan_SetCursor)(_ptr_ this_, _int_ frame, _int_ type);
#define b_MouseMan_SetCursor(frame, type) ((_func_MouseMan_SetCursor)0x50CEA0)(o_MouseMgr,(_int_)(frame),(_int_)(type))

#define o_GetMapCellVisability(x,y,z) CALL_3(_dword_, __fastcall, 0x4F8040, x, y, z)


// Максимальное случайное число.
#define RandMax 0x7FFF

// Случайное число без диапазона.
#define Rand() CALL_0(_dword_, __cdecl, 0x61842C)

// Получения случайного double.
inline double DoubleRand(double low, double high)
{
  return low + (double)Rand()/(double)RandMax*(high - low);
}


// Полувремязависимый ранодом.
// В зависиости от знчения своей управляющей переменной генерирует случайное целое число в указанных пределах по seed или текущему времени.
#define TRandint(Low, High) CALL_2(_int_, __fastcall, 0x50B3C0, (_int_)Low, (_int_)High)


// Случайное целое число в диапазоне.
#define Randint(Low, High) CALL_2(_int_, __fastcall, 0x50C7C0, (_int_)Low, (_int_)High)


// Ожидание до указанного времени (или времени, превышающего указанное).
#define WaitTill(Time) CALL_1(void, __fastcall, 0x4F8980, (_dword_)Time);


// Начало тактильного эффекта (IFC20.dll, не проявляется при стандартных средствах управления).
#define TouchEffectStart(Name, a2) CALL_2(void, __fastcall, 0x4B6750, (_cstr_)Name, (_dword_)a2)

// Получение имени существа по его номеру и количеству.
#define GetCreatureName(Type, Count) CALL_2(_cstr_, __fastcall, 0x43FE20, (_int_)Type, (_int_)Count)

// Получение грейда существа (-1 - нет грейда).
#define GetCreatureGrade(Type) CALL_1(_int_, __fastcall, 0x47AAD0, (_int_)Type)


// *************************************








NOALIGN struct _GarrisonBar_
{
 _byte_ field_0[28];
 _int_ x; //+28
 _int_ y; //+32
 _bool_ is_down_bar; //+36
 _int_ owner_id; //+40
 _int_ sel_slot_index;
 _byte_ field_2C[56];
 _Dlg_* parent_dlg;
 _Army_* army; //+108
 _int_ hero_pic; //+112
 _Hero_* hero; //+116

 // normal
 inline void Update(_bool_ a2, _int_ a3) { CALL_3(void, __thiscall, 0x5AA0C0, this, a2, a3); }
 inline void UpdateRedraw(_int_ a2) { CALL_2(void, __thiscall, 0x5AA090, this, a2); }
};

NOALIGN struct _TownMgr_
{
 _byte_ field_0[56];
 
 _Town_* town; //+56
 _byte_ field_48[220];
 _Dlg_* dlg; // +280
 _GarrisonBar_* garribar_up; //+284
 _GarrisonBar_* garribar_down; //+288

 _GarrisonBar_* garribar_sel; //+292
 _int_ garribar_slot_index_sel; //+296

 _GarrisonBar_* garribar_src; //+300
 _int_ garribar_slot_index_src; // +304
 _GarrisonBar_* garribar_dst; //+308
 _int_ garribar_slot_index_dst; //+312

 _ptr_ resources_bar;//+316

 _dword_ field_140;//+320

 _char_ statusbar_text[88]; // +324

 _int_ command_code; //+412
 
 _byte_ f1A0[56]; // +1A0h


 // normal

 void CreateNewGarriBars() {CALL_1(void, __thiscall, 0x5C7210, this);}
 void MoveHeroUp() {CALL_1(void, __thiscall, 0x5D5550, this);}
 void MoveHeroDown() {CALL_1(void, __thiscall, 0x5D5620, this);}

 // my 

 inline void DeleteGarriBars()
 {
  if (this->garribar_up) o_Delete(this->garribar_up); this->garribar_up = NULL;
  if (this->garribar_up) o_Delete(this->garribar_down); this->garribar_down = NULL;
 }

 inline void SwapHeroes() {this->town->SwapHeroes(); DeleteGarriBars(); CreateNewGarriBars();}

};


typedef void (__thiscall * _func_AdvMgr_ActivateHero)(_ptr_ this_, int hero_id, int a3, char a4, char a5);
#define o_AdvMgr_ActivateHero(advmgr, hero_id, a3, a4, a5) ((_func_AdvMgr_ActivateHero)0x417A80)((_ptr_)(advmgr), (int)(hero_id), (int)(a3), (char)(a4), (char)(a5))

typedef _MapItem_* (__thiscall * _func_GetMapItem)(_ptr_ this_, int x, int y, int z);
#define b_GetMapItem(x,y,z) ((_func_GetMapItem)0x4086D0)(*(_ptr_*)(o_AdvMgr + 92),(int)(x),(int)(y),(int)(z))



NOALIGN struct _AdvMgr_
{
 _byte_ field_0[68]; //+0
 _Dlg_* dlg; //+68
 _byte_ field_48[20]; //+72
 _GameMap_* map; //+92
 _byte_ field_60[488]; //+96
 // Фоновые звуки (ссылка на звук или 0, если не проигрывается).
 _Wav_* loop_sounds[70]; // +584
 _byte_ field_360[52]; //+864
 _int_ current_info_panel_id;//+916
 
 _byte_ f398[32]; // +398h
 
 
 //inline _Dlg_* GetDlg() {return *(_Dlg_**)((_ptr_)this + 68);}
 //inline _int_ GetCurrentInfoPanelID() {return *(_int_*)((_ptr_)this + 916);}
 inline _dword_ FullUpdate(_bool_ redraw_screen) {return CALL_3(_dword_, __thiscall, 0x417380, this, redraw_screen, 0/*not used*/);}
 inline _dword_ RebuildHeroInfoPanel(_bool_ even_if_on_top) {return CALL_2(_dword_, __thiscall, 0x4163B0, this, even_if_on_top);}
 inline _dword_ RebuildTownInfoPanel(_bool_ even_if_on_top) {return CALL_2(_dword_, __thiscall, 0x416450, this, even_if_on_top);}
 inline _dword_ UpdateInfoPanel(_bool_ even_if_on_top, _bool_ redraw, _bool_ redraw_screen)
  {return CALL_4(_dword_, __thiscall, 0x415D40, this, even_if_on_top, redraw, redraw_screen);}
 inline _dword_ RedrawInfoPanel(_bool_ redraw_screen) {return CALL_2(_dword_, __thiscall, 0x402BC0, dlg, redraw_screen);}
 inline _dword_ ViewWorld(_int_ a2, _int_ a3) {return CALL_3(_dword_, __thiscall, 0x5FC340, this, a2, a3);}
 inline _dword_ ShowPuzzleMap() {return CALL_1(_dword_, __thiscall, 0x41A750, this);}
 inline _dword_ DrawMap(_int_ x, _int_ y, _int_ z, _int_ a5, _bool_ update_info_panel)
  { return CALL_6(_dword_, __thiscall, 0x40F350, this, x,y,z, a5, update_info_panel);}
 inline _dword_ DrawMap(_dword_ xyz, _int_ a5, _bool_ update_info_panel)
  { return DrawMap(b_unpack_x(xyz), b_unpack_y(xyz), b_unpack_z(xyz), a5, update_info_panel);}
 inline _dword_ Dig(_int_ x, _int_ y, _int_ z) { return CALL_4(_dword_, __thiscall, 0x40EBF0, this, x,y,z); }
 inline _dword_ Dig() { return Dig(-1,0,0); }
 
 
};


#define o_SwapMan_SplitArmyStack (*(_bool_*)0x6A3D68)
NOALIGN struct _SwapMan_
{
 _byte_ field_0[56];
 _Dlg_* dlg; //+56
 _Pcx8_* pcx8_sel_rect; //+60
 _Hero_* hero[2]; //+64
 _int_ src_hero_ix; //+72
 _int_ dst_hero_ix; //+76
 _int_ src_slot_ix; //+80
 _int_ dst_slot_ix; //+84
 _bool_ slot_selected; //+88
};

//////////////////////////////////////////////////////////////////////////////////////////
//typedef _int8_ (__thiscall *_func_AdvMgr_FullUpdate)(_ptr_ this_, _byte_ redraw, _dword_ unused);
//#define b_AdvMgr_FullUpdate(redraw) ((_func_AdvMgr_FullUpdate)0x417380)(o_AdvMgr, (_byte_)(redraw), 0)

//typedef char (__thiscall * _func_AdvMgr_RebuildHeroInfoPanel)(_ptr_ advmgr, _int8_ a2);
//typedef char (__thiscall * _func_AdvManInfoPanel_Redraw)(_ptr_ this_, _int8_ a2);
//#define o_AdvMgr_RebuildHeroInfoPanel(this_, even_if_not_on_top) ((_func_AdvMgr_RebuildHeroInfoPanel)0x4163B0)((_ptr_)(this_),(_int8_)(even_if_not_on_top))
//#define b_AdvMgr_RedrawInfoPanel(this_, redraw_on_primary_buf) ((_func_AdvManInfoPanel_Redraw)0x402BC0)(*(_ptr_*)(this_ + 68), (_int8_)(redraw_on_primary_buf))

//#define b_AdvmanHeroInfoPanel_Update(draw) {((_func_RebuildAdvmanHeroInfoPanel)0x4163B0)(o_AdvMgr, 1); ((_func_RedrawAdvmanInfoPanel)0x402BC0)(*(_ptr_*)(o_AdvMgr + 68), (_int8_)(draw));}
//#define b_AdvMgr_CurrentInfoPanelID (*(_dword_*)(o_AdvMgr + 916))

//typedef void (__thiscall * _func_AdvMgr_ViewWorld)(_ptr_ this_, _int_ a2, _int_ a3);
//#define b_AdvMgr_ViewWorld(a2, a3) ((_func_AdvMgr_ViewWorld)0x5FC340)(o_AdvMgr, (_int_)(a2), (_int_)(a3))
//#define b_AdvMgr_ShowPuzzleMap() ((_func_this)0x41A750)(o_AdvMgr)

//typedef void (__thiscall *_func_AdvMgr_DrawMap)(_ptr_ this_, _int_ x, _int_ y, _int_ z, _bool_ a5, _bool_ a6);
//#define o_AdvMgr_DrawMap(this_,x,y,z,a5,a6) ((_func_AdvMgr_DrawMap)0x40F350)((_ptr_)(this_),(_int_)(x),(_int_)(y),(_int_)(z),(_bool_)(a5),(_bool_)(a6))
//#define b_AdvMgr_DrawMap(this_,xyz,a5,a6) o_AdvMgr_DrawMap(this_, b_unpack_x(xyz), b_unpack_y(xyz), b_unpack_z(xyz),(_bool_)(a5),(_bool_)(a6))


#define o_Terminate(pchar_reason_text) CALL_1(void, __fastcall, 0x4F3D20, pchar_reason_text)

//typedef void (* _func_PrintToScreenLog)(_ptr_ screen_log, char* format, ...);
//#define b_PrintToScreenLog(format, ...) ((_func_PrintToScreenLog)0x553C40)(o_ScreenLogStruct, (char*)(format), __VA_ARGS__)
#define b_PrintToScreenLog(format, ...) ((void (__cdecl *)(_ptr_, char*, ...))0x553C40)(o_ScreenLogStruct, (char*)(format), __VA_ARGS__)

typedef int (__cdecl * _func__beginthread)(_ptr_ func, SIZE_T dwStackSize, _ptr_ arg);
#define o__beginthread(func, stack_size, arg) ((_func__beginthread)0x61A56C)((_ptr_)(func), (SIZE_T)(stack_size), (_ptr_)(arg))




//////typedef LRESULT (CALLBACK *_func_WndProc)(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
//////#define o_WndProc(hWnd, message, wParam, lParam) ((_func_WndProc)0x4F8290)((HWND)(hWnd),(UINT)(message),(WPARAM)(wParam),(LPARAM)(lParam))


NOALIGN struct _InputMgr_
{
 __declspec(noinline) void SendEventMsg(int type, int subtype, int id, int x, int y, int flags, int flags_2, int new_param) // +0h
 {
  if ( (_ptr_)this )
  {
   if ( *((_ptr_ *)(_ptr_)this + 13) == 1 )
   {
    _ptr_ v = 32 * *((_ptr_ *)(_ptr_)this + 527);
    _EventMsg_ * msg = (_EventMsg_ *)((_ptr_)this + v + 56);
    *(_ptr_ *)((_ptr_)this + v + 68) = 0;

    ///
    msg->type = type;
    msg->subtype = subtype;
    msg->item_id = id;
    msg->x_abs = x;
    msg->y_abs = y;
    msg->flags = flags;
    msg->flags_2 = flags_2;
    msg->new_param = new_param;
    ///

    ++*((_ptr_ *)(_ptr_)this + 527);
    *((_ptr_ *)(_ptr_)this + 527) %= 64;
    if ( *((_ptr_ *)(_ptr_)this + 526) == *((_ptr_ *)(_ptr_)this + 527) )
    {
     *((_ptr_ *)(_ptr_)this + 526) += 1;
     *((_ptr_ *)(_ptr_)this + 526) %= 64;
    }
    *((_ptr_ *)(_ptr_)this + 597) = 0;
   }
  }
 }
 
 
 _byte_ f4[48]; // +4h
 
 // Возможен ли приём событий.
 _bool32_ CanRecieveMsg; // +34h
 // Принятые события.
 // При окончании массива он начинает заполняться сначала (т. е. будут затираться самые ранние события).
 _EventMsg_ Msg[64]; // +38h
 // Номер первого события для обработки.
 // При переполнении массива будет изменяться, чтобы всегда указывать на самое раннее сообщение.
 _int_ FirstMsg_ix; // +838h
 // Номер события, в которое запишется следующее принятое.
 _int_ NextMsg_ix; // +83Ch
 
 _byte_ f840[288]; // +840h
 
 
 // Взятие первого события из очереди.
 // out_event_msg - адрес сообщения, в которое запишется информация.
 inline void Peek_Event(_EventMsg_* out_event_msg)
 {
   CALL_2(void, __thiscall, 0x4EC660, this, out_event_msg);
 }
 
};





// Менеджер звука (размер неизвестен, пока - для описания методов).
NOALIGN struct _SoundMgr_
{
  _byte_ f0[216]; // +0h
  
  // Старт проигрывания звука (возвращения индекса среди проигрываемых звуков).
  inline _dword_ StartSample(_Wav_* Wav)
  {
    return CALL_2(_dword_, __thiscall, 0x59A510, this, Wav);
  }
};





// ********************************************


// Состояние банка существ.
// Каждый банк может быть в одном из четырёх состояний, случайно выбирающемся в начале игры.
// Состояние определяет охрану и награду банка.
NOALIGN struct _CrBankState_
{
  // Защитники банка.
  _Army_ defenders; // +0h
  
  // Количества ресурсов, входящих в составе награды.
  _int_ resources_award[7]; // +38h
  
  // Тип существа, входящего в состав награды.
  _int_ creatures_award_ix; // +54h
  // Количество существ соответствующего типа, входящее в состав награды.
  _byte_ creatures_award_count; // +58h
  
  // Абсолютное значение шанса выпадения данного состояния.
  // Реальный шанс зависит от абсолютных значений всех состояний, но обычно они в сумме дают 100, т. е. это проценты.
  _byte_ chance; // +59h
  
  // Шанс появления улучшенного стека среди защитников банка при взятии в процентах.
  _byte_ upgrade_chance; // +5Ah
  
  // Количество артефактов-сокровищ, входящих в состав награды.
  _byte_ arts_treasure_count; // +5Bh
  // Количество малых артефактов, входящих в состав награды.
  _byte_ arts_minor_count; // +5Ch
  // Количество великих артефактов, входящих в состав награды.
  _byte_ arts_major_count; // +5Dh
  // Количество артефактов-реликтов, входящих в состав награды.
  _byte_ arts_relic_count; // +5Eh
  
  // Результат выравнивания, не используется.
  _byte_ dummy_f5F[1]; // +5Fh
};



// Тип банка существ как объекта на карте.
// Хранит в себе возможные состояния банка существ (т. е. его охрану и награду).
NOALIGN struct _CrBankType_
{
  // Всегда 1.
  _byte_ setup; // +0h
  
  // Результат выравнивания, не используется.
  _byte_ dummy_f1[3]; // +1h
  
  // Имя банка.
  _HStringA_ name; // +4h
  
  // Возможные состояния банка.
  _CrBankState_ States[4]; // +Ch
  
  
  
  
  
  
  // Конструктор.
  inline _CrBankType_* Contruct()
  {
    return CALL_1(_CrBankType_*, __thiscall, 0x47A400, this);
  }
};



// ********************************************


// Внешнее жилище на корте.
NOALIGN struct _Dwelling_
{
  // Тип жилища как объекта (17 - обычное, 20 - с 4 существами).
  _byte_ type;
  // Подтип жилища как объекта.
  _byte_ subtype;
  _byte_ f2;
  _byte_ f3;
  // Типы существ, доступных для найма (-1 - нет).
  _int_  creature_types[4];
  // Количества существ, доступных для найма.
  _word_ creature_counts[4];
  // Защитники жилища.
  _Army_ defenders;
  // X-координата на карте.
  _byte_ x;           // +54 db (3)
  // Y-координата на карте.
  _byte_ y;           // +55 db
  // Находится ли в подземелье.
  _bool8_ l;
  // Игрок-хозяин (-1 - нет).
  _int8_ owner_ix;
  _byte_ f58;
  _byte_ f59;
  _byte_ f5A;
  _byte_ f5B;
  
  // Конструктор.
  inline _Dwelling_* Contruct()
  {
    return CALL_1(_Dwelling_*, __thiscall, 0x4B8250, this);
  }
};

// ********************************************



















// 12 bytes. Сокровище зоны генератора случйных карт.
NOALIGN struct _ZoneTreasure_
{
  // +0. Минимальная ценность.
  _int32_ min_value;
  
  // +4. Минимальная ценность.
  _int32_ max_value;
  
  // +8. Частота появления.
  _int32_ density;
};


// 28 bytes. Связь зоны  генератора случйных карт с другой.
NOALIGN struct _ZoneConnection_
{
  // +0. Настройки зоны, с которой связана текущая.
  _ZoneSettings_ *another_zone_settings;
  
  // +4. Ценность прохода.
  _int32_ value;
  
  // +8. Широкий ли проход.
  _bool8_ is_wide;
  
  // +9. Охраняется ли стражем границы.
  _bool8_ has_border_guard;
  
  // +10. Была ли уже создана на заготовке карты.
  _bool8_ is_created;
  
  // +11. Результат выравнивания, не используется.
  _byte_ dummy_f11[1];
  
  // +12. Минимальное количество зон человека, необходимое для появления связи.
  _int32_ min_human_pos;
  
  // +16. Максимальное количество зон человека, подходящее для появления связи.
  _int32_ max_human_pos;
  
  // +20. Минимальное количество зон игроков, необходимое для появления связи.
  _int32_ min_total_pos;
  
  // +24. Максимальное количество зон игроков, подходящее для появления связи.
  _int32_ max_total_pos;
};


// 212 bytes. Настройки зоны генератора случйных карт.
NOALIGN struct _ZoneSettings_
{
  // +0. Номер зоны.
  _int32_ id;
  
  // +4. Тип зоны (0 - зона человека, 1 - зона ИИ, 2 - сокровищницы, 3 - пустая).
  _int32_ type;
  
  // +8. Базовый размер зоны.
  _int32_ base_size;
  
  // +12. Минимальное количество зон человека, необходимое для появления зоны.
  _int32_ min_human_pos;
  
  // +16. Максимальное количество зон человека, подходящее для появления зоны.
  _int32_ max_human_pos;
  
  // +20. Минимальное количество зон игроков, необходимое для появления зоны
  _int32_ min_ai_pos;
  
  // +24. Максимальное количество зон игроков, подходящее для появления зоны.
  _int32_ max_ai_pos;
  
  // +28. Игрок - владелец зоны.
  _int32_ owner;
  
  // +32. Минимальное количество городов без форта игрока.
  _int32_ min_player_towns;
  
  // +36. Минимальное количество городов с фортом игрока.
  _int32_ min_player_castles;
  
  // +40. Частота появления городов без форта игрока.
  _int32_ player_towns_density;
  
  // +44. Частота появления городов с фортом игрока.
  _int32_ player_castles_density;
  
  // +48. Минимальное количество нейтральных городов без форта.
  _int32_ min_neutral_towns;
  
  // +52. Минимальное количество нейтральных городов с фортом.
  _int32_ min_neutral_castles;
  
  // +56. Частота появления нейтральных городов без форта.
  _int32_ neutral_towns_density;
  
  // +60. Частота появления нейтральных городов с фортом.
  _int32_ neutral_castles_density;
  
  // +64. Должны ли все города зоны принадлежать одному типу.
  _bool8_ towns_are_the_same_type;
  
  // +65. Доступность городов.
  _bool8_ towns_aval[9];
  
  // +74. Результат выравнивания, не используется.
  _byte_ dummy_f4A[2];
  
  // +76. Минимальное количество шахт каждого типа.
  _int32_ min_mines[7];
  
  // +104. Частота появления шахт каждого типа.
  _int32_ mines_density[7];
  
  // +132. Соответствует ли тип территории родной земле города.
  _bool8_ terr_match_to_town;
  
  // +133. Доступности типов территорий.
  _bool8_ terrs_aval[8];
  
  // +141. Результат выравнивания, не используется.
  _byte_ dummy_f8D[3];
  
  // +144. Сила монстров (0 - нет монстров, 1 - слабые, 2 - средние, 3 - сильные).
  _int32_ monsters_strength;
  
  // +148. Соответствуют ли типы монстров типу города.
  _bool8_ monsters_match_to_town;
  
  // +149. Доступности монстров из городов.
  _bool8_ monsters_towns_aval[9];
  
  // +158. Результат выравнивания, не используется.
  _byte_ dummy_f9E[2];
  
  // +160. Сокровища зоны.
  _ZoneTreasure_ treasure[3];
  
  // +196. Связи с другими зонами.
  _List_<_ZoneConnection_> connections;
};







// ********************************************// ********************************************

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//typedef _ptr_ (__thiscall * _func_GetNetMessage)(_ptr_ this_, _int8_ a2, _int_ a3);
//#define b_GetNetMessage() ((_func_GetNetMessage)0x553440)(o_DirectPlayCOMObject, 0, 0);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




// Переписанная геройская функция загрузки и проигрывания звука.
_Sample_ __fastcall rwr_Load_And_Start_Sample(_cstr_ WavName);




//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
