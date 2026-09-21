#define _CRT_SECURE_NO_WARNINGS
#define _SECURE_SCL 0
#define _HAS_ITERATOR_DEBUGGING 0
#pragma warning(disable : 4005)
#pragma warning(disable : 4010)

#include "stdafx.h"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>
#include "consts.h"
#include "HotA\homm3.h"

// ===============================================================
// ------------------------- API (WIP) ---------------------------
// ---------------------------------------------------------------
#define gpGame (*(game**)0x699538)
#define pWindowManager (*(heroWindowManager**)0x6992D0)
#define tinyFont (*(font**)0x698A54)
#define GameText (*(TTextResource**)0x6A5DC4)
#define AGRText ((char**)0x6A5E7C)
#define gTownTypeNames ((char**)0x6A755C)

inline void debugStr(const char* format, ...);
inline void debugStrWin(const char* format, ...);

#pragma pack(push, 4)
class stdString
{
public:
	void *allocator;
	const char *text;
	unsigned int length;
	unsigned int capacity;
};

class stdDequeIterator
{
public:
   int* first;
   int* last;
   int* next;
   int* map;
};

class stdDeque
{
public:
   char allocator;
   stdDequeIterator itFirst;
   stdDequeIterator itLast;
   int *map;
   int mapSize;
   int size;
};

// Heroes III was built with Visual C++ 6.0. Its std::vector object contains
// an allocator/padding dword followed by the three usual pointers (16 bytes
// total on x86). Modern MSVC std::vector is only 12 bytes and must never be
// embedded in an overlay of a game-owned object.
template<class T>
struct exe_vector
{
   std::uint32_t allocator_or_pad;
   T* first;
   T* last;
   T* end;

   std::size_t size() const
   {
      const std::uintptr_t beginAddress = reinterpret_cast<std::uintptr_t>(first);
      const std::uintptr_t endAddress = reinterpret_cast<std::uintptr_t>(last);

      if (!beginAddress || endAddress < beginAddress ||
          (endAddress - beginAddress) % sizeof(T) != 0)
         return 0;

      return (endAddress - beginAddress) / sizeof(T);
   }
};

static_assert(sizeof(void*) == 4, "NewSpells supports only the 32-bit Heroes III executable");
static_assert(sizeof(exe_vector<void*>) == 0x10, "Heroes III vector ABI mismatch");
static_assert(std::is_standard_layout<exe_vector<void*> >::value,
   "Heroes III vector view must remain standard-layout");
static_assert(std::is_trivially_destructible<exe_vector<void*> >::value,
   "Heroes III vector view must never free game-owned storage");

class baseManager
{
public:
	void *vftable;
	baseManager *nextManager;
	baseManager *prevManager;
	int id;
	int priority;
	char cMgrName[32];
	int status;
};

class resource
{
public:
	void *vftable;
	char Name[12];
	bool Unknown1;
	int resType;
	int ReferenceCount;
};

class TXT_File
{
public:
	char *message[765];
};

class TTextResource : public resource
{
public:
	char *Data;
	TXT_File *text;
};

class TPalette16 : public resource
{
public:
	short Palette[256];
};

class TPalette24 : public resource
{
public:
	short Palette[384];
};

class Bitmap16Bit : public resource
{
public:
	int DataSize;
	int ImageSize;
	int Width;
	int Height;
	int ScanlineSize;
	unsigned char *map;
	bool keepData;
};

class Bitmap816 : public resource
{
public:
	int DataSize;
	int ImageSize;
	int Width;
	int Height;
	int Pitch;
	unsigned char *map;
	TPalette16 Palette;
	TPalette24 Palette24;

	inline void Draw(int sx, int sy, int sw, int sh, Bitmap16Bit *dst, int d_X, int d_Y, bool tblit)
	{
		CALL_9(void, __thiscall, 0x44FA80, this, sx, sy, sw, sh, dst, d_X, d_Y, tblit);
	}
};


class myABC
{
public:
	int abcA;
	unsigned int abcB;
	int abcC;
};

class TFontSpec
{
public:
	unsigned char first;
	unsigned char last;
	unsigned char depth;
	char xspace;
	char yspace;
	unsigned char height;
	char baseyoffset;
	char pad;
	int numpal;
	unsigned short pal[10];
	myABC abc[256];
	unsigned long Offset[256];
};

class font : public resource
{
public:
	enum TColor
	{
		PRIMARY = 1,
		PRIMARY_HIGHLIGHT = 2,
		PRIMARY_DIM = 3,
		WHITE = 4,
		WHITE_HIGHLIGHT = 5,
		WHITE_DIM = 6,
		HEADING = 7,
		HEADING_HIGHLIGHT = 8,
		HEADING_DIM = 9,
		WHITE_PLAYER = 10,
		WHITE_PLAYER_HIGHLIGHT = 11,
		WHITE_PLAYER_DIM = 12,
		CHAT = 13,
		CHAT_HIGHLIGHT = 14,
		CHAT_DIM = 15,
		LowestColor = 1,
		HighestColor = 14,
		CUSTOM_COLOR = 256
	};

	enum EJustify
	{
		LEFT_JUSTIFIED = 0,
		CENTER_JUSTIFIED = 1,
		RIGHT_JUSTIFIED = 2,
		TOP_JUSTIFIED = 0,
		VERT_CENTER_JUSTIFIED = 4,
		BOTTOM_JUSTIFIED = 8
	};

	TFontSpec fr;
	TPalette16 p16;

	inline void DrawBoundedString(const char *str, Bitmap16Bit *bitmap, int x, int y, int boxWidth, int boxHeight, TColor color_scheme, unsigned int justification, int cursorPos) const
	{
		CALL_10(void, __thiscall, 0x4B51F0, this, str, bitmap, x, y, boxWidth, boxHeight, color_scheme, justification, cursorPos);
	}
};

class CSpriteFrame : public resource
{
public:
	int DataSize;
	int ImageSize;
	TEncodingMethod EncodingMethod;
	int Width;
	int Height;
	int CroppedWidth;
	int CroppedHeight;
	int CroppedX;
	int CroppedY;
	int Pitch;
	unsigned char *map;
};

class CSequence
{
public:
	int numFrames;
	int allocatedFrames;
	CSpriteFrame** f;
};

class CSprite : public resource
{
public:
	CSequence** s;
	TPalette16* p16;
	TPalette24* p24;
	int numSequences;
	int* validSeqMask;
	int Width;
	int Height;

	inline void Draw(int seqnum, int framenum, int sx, int sy, int sw, int sh, void *dst, int dx, int dy, int dw, int dh, int dpitch, bool hflip, bool tblit)
	{
		CALL_15(void, __thiscall, 0x47B610, this, seqnum, framenum, sx, sy, sw, sh, dst, dx, dy, dw, dh, dpitch, hflip, tblit);
	}
};

class heroWindow;

class widget
{
public:
	struct
	{
		void* (*scalar_deleting_destructor)(unsigned int flags);
		void* _purecall[3];
		int (*GetRealHeight)();
		int (*GetRealWidth)();
		void (*process_hover)();
		void (*Dim)();
		void (*enable)(bool arg);
		void (*Log)(char *msg, ...);
		void *empty_virtual_function;
	} *vftable;

	heroWindow *parentWindow;
	widget *prevWidget;
	widget *nextWidget;
	short id;
	short priority;
	short style;
	short status;
	short x;
	short y;
	short width;
	short height;
	int focusable;
	unsigned char on;
	char* RollOver;
	char* RightClick;
	unsigned char freeText;
};

class heroWindow
{
public:
	void *vftable;					// 0x00
	int priority;					// 0x04
	heroWindow* nextWindow;			// 0x08
	heroWindow* prevWindow;			// 0x0C
	int type;						// 0x10
	int status;						// 0x14
	int x;							// 0x18						
	int y;							// 0x1C
	int width;						// 0x20
	int height;						// 0x24
	widget* headWidget;				// 0x28
	widget* tailWidget;				// 0x2C
	std::vector<widget*> Widgets;	// 0x30
	int focusId;					// 0x40
	Bitmap16Bit* background;		// 0x44
	int DeactivatesCount;			// 0x48
									// 0x4C

	inline widget* GetWidget(int id) const
	{
		return CALL_2(widget*, __thiscall, 0x5FF5B0, this, id);		
	}
};

class heroWindowManager : public baseManager
{
public:
	int dialogReturn;
	int lastHover;
	Bitmap16Bit* screenBitmap;
	int colorCyclingOn;
	int isWaitingForFadeIn;
	heroWindow* activeWindow;
	heroWindow* lastActive;
	Bitmap16Bit* bmpFizzleSource;
	heroWindow* headWindow;
	heroWindow* tailWindow;
};

class CHeroWindowEx : public heroWindow
{
public:
	int m_lastIMHoverID;
	int lastHover;
};

class CAdvPopup : public CHeroWindowEx
{
public:
	int exitId;
	int exitCodeX;
	int exitCommand;
};

class textWidget : public widget
{
public:
	stdString Text;
	font* Font;
	font::TColor Color;
	font::TColor BackColor;
	font::EJustify Justify;
};

class CTextEntrySave
{
public:

};

class textEntryWidget : public textWidget
{
public:
	Bitmap816* textBack;
	CTextEntrySave* saveBack;
	unsigned short cursorIndex;
	unsigned short bufferSize;
	short textWidth;
	short textHeight;
	short textX;
	short textY;
	short textLines;
	short attributes;
	short type;
	short displayOffset;
	char cursorFlashOn;
	unsigned char focus;
	unsigned char autoDraw;
};

class GameSelectionHeadersStruct
{
public:

};

class CNewPlayerUpdateMan
{
public:

};

class CNetPlayerHandler
{
public:
	void* vftable;
};

class CChatEdit : public textEntryWidget
{
public:

};

class slider : public widget
{
public:
	enum EGraphics
	{
		BROWN = 0,
		BLUE = 1
	};

	int knobPos;
	int currentState;
	int pageSize;
	int numStates;
	CSprite* sliderSprite;
	Bitmap816* sliderBitmap;
	int oldState;
	int knobRange;
	int length;
	long knob_start;
	short clickX;
	short clickY;
	unsigned char hotKeys;
	unsigned char scrolling;
	int lastFocus;
	void (*sliderFunction)(int, heroWindow*);
};

class button : public widget
{
public:
	CSprite* buttonIcon;
	int normalFrame;
	int selectedFrame;
	int disabled_frame;
	unsigned char endDialog;
	std::vector<int> hotKeyCodes;
	stdString Text;
};

class textButton : public button
{
public:
	font* Font;
	font::TColor textColor;
};

class CSaveScreen : public Bitmap16Bit
{
public:
	unsigned char screenSaved;
	int m_x;
	int m_y;
};

class CSingleSelectionNetMsgHandler
{
public:
	void* vftable;
};

/*class TSingleSelectionWindow : public CAdvPopup 
{
public:
	char my_index;
	char pos_copy;
	char hero_pos[8];
	unsigned char loadMode;
	textWidget* human;
	textWidget* text;
	widget* handicap;
	widget* townL;
	widget* townR;
	widget* heroL;
	widget* heroR;
	widget* resL;
	widget* resR;
	widget* name;
	widget* flag;
	widget* face;
	widget* town;
	widget* bonus;
	textEntryWidget* nameEdit;
	unsigned long freeBlocks;
	unsigned long neededBlocks;
	int SavePart;
	char tempFilename[13];
	long tempBufStart;
	int SelectedVMPort;

	enum EWidgetIDs
	{
	};

	unsigned long clickTime;
	GameSelectionHeadersStruct* SelectionHeaders;
	int num_mapFiles;
	int max_maps;
	int* mapFilter;
	int mapsInFilter;
	unsigned char loadGameMode;
	unsigned char saveGameMode;
	int textIndex;
	CSprite* VictoryIcon;
	CSprite* LossIcon;
	CSprite* TownPix;
	CSprite* Resource;
	CSprite* heroSpecificAbility;
	Bitmap816* GoldBox;
	Bitmap816* Flags[8];
	Bitmap816* Panels[8];
	Bitmap816* HeroPix[156];
	Bitmap816* randomTownQuestion;
	Bitmap816* randomHeroQuestion;
	Bitmap816* randomTown;
	Bitmap816* randomHero;
	Bitmap816* noDice;
	Bitmap816* noHero;
	int sortDirection;
	int currentIndex;
	int currentMap;
	int durationIndex;
	unsigned char inAdvancedOptions;
	unsigned char inScenarioOptions;
	textEntryWidget* saveGameEdit;
	unsigned char mode;
	CNewPlayerUpdateMan* pNewPlayerUpdateMan;
	CNetPlayerHandler netPlayerHandler;
	unsigned char receivedMaps;
	slider* chatSlider;
	slider* fileSlider;
	slider* durationSlider;
	slider* nameSlider;
	textWidget* chatWidget;
	textWidget* nameList1;
	textWidget* nameList2;
	unsigned char mapChanged;
	unsigned char readingMaps;
	CChatEdit* chatEdit;
	int sortWhich;
	int filterSize;
	unsigned char scenarioOptionsStarted;
	unsigned char chatShowing;
	textButton* chatToggle;
	unsigned char receivingMaps;
	CSaveScreen* flagBack;
	char gameVersion[20];
	CSingleSelectionNetMsgHandler netMsgHandler;

	enum EOtherWidgetIDs
	{
	};

	enum
	{
		NWIDGETS = 211
	};
};*/

class type_artifact
{
public:
	TArtifact type;
	SpellID spell;
};

class SCampaign
{
public:
	char gap[124];
};

class SGameSetupOptions
{
public:
	char color[8];
	char handicap[8];
	TTownType alignment[8];
	char playerPos[8];
	char difficulty;
	char cFilename[351];
	char canFlipFromToComputer[8];
	char curSelectedPlayer;
	char bThisFileInitialized;
	char initializationNumHumans;
	char turnDuration;
	THeroID startingHero[8];
	char startingBonus[8];
};

struct VictoryConditionStruct
{
	char Type;
	char AllowNormalVictory;
	char AppliesToComputer;
	int ArtifactNum;
	int CreatureType;
	int NumCreatures;
	EGameResource ResourceType;
	int NumResources;
	int TownX;
	int TownY;
	int TownZ;
	char HallLevel;
	char CastleLevel;
	int HeroX;
	int HeroY;
	int HeroZ;
	THeroID HeroID;
	int MonsterX;
	int MonsterY;
	int MonsterZ;
	int time_to_survive;
	unsigned char GameWon;
	char playerWinner;
};

struct LossConditionStruct
{
	char Type;
	int TownX;
	int TownY;
	int TownZ;
	int HeroX;
	int HeroY;
	int HeroZ;
	THeroID HeroID;
	short NumDays;
	unsigned char GameLost;
	char playerLoser;
};

struct mapCellDefaultObject
{
	unsigned int ID;
};

struct mapCellArtifact
{
	unsigned int Type : 4;
	unsigned int _u1 : 15;
	unsigned int ID : 12;
	unsigned int HasSetup : 1;
};

struct mapCellCampfire
{
	unsigned int ResType : 4;
	unsigned int ResValue : 28;
};

struct mapCellCorpse
{
	unsigned int ID : 5;
	unsigned int _u1 : 1;
	unsigned int ArtifactID : 10;
	unsigned int HasArtifact : 1;
	unsigned int _u2 : 15;
};

struct mapCellCreatureBank
{
	unsigned int _u1 : 5;
	unsigned int Visited : 8;
	unsigned int ID : 12;
	unsigned int Taken : 1;
	unsigned int _u2 : 6;
};

struct mapCellEvent
{
	unsigned int ID : 10;
	unsigned int Enabled : 8;
	unsigned int AIEnabled : 1;
	unsigned int OneVisit : 1;
	unsigned int _u1 : 12;
};

struct mapCellFlotsam
{
	enum EMapCellFlotSam
	{
		FLOTSAM_EMPTY = 0,
		FLOTSAM_WOOD5 = 1,
		FLOTSAM_WOOD5_GOLD200 = 2,
		FLOTSAM_WOOD10_GOLD500 = 3,
	};

	EMapCellFlotSam Type;
};

struct mapCellFountainFortune
{
	unsigned int _u1 : 5;
	unsigned int Visited : 8;
	int BonusLuck : 4;
	unsigned int _u2 : 15;
};

struct mapCellLeanTo
{
	unsigned int ID : 5;
	unsigned int _u1 : 1;
	unsigned int ResValue : 4;
	unsigned int ResType : 4;
	unsigned int _u2 : 18;
};

struct mapCellMagicShrine
{
	unsigned int _u1 : 13;
	unsigned int Spell : 10;
	unsigned int _u2 : 9;
};

struct mapCellMagicSpring
{
	unsigned int ID : 5;
	unsigned int _u1 : 1;
	unsigned int Used : 1;
	unsigned int _u2 : 25;
};

struct mapCellMonster
{
	unsigned int Amount : 12;
	int Aggression : 5;
	unsigned int NoRun : 1;
	unsigned int NoGrowth : 1;
	unsigned int SetupIndex : 8;
	unsigned int GrowthRemainder : 4;
	unsigned int HasSetup : 1;
};

struct mapCellMysticGarden
{
	unsigned int id : 5;
	unsigned int _u1 : 1;
	unsigned int ResType : 4;
	unsigned int HasRes : 1;
	unsigned int _u2 : 21;
};

struct mapCellPandorasBox
{
	unsigned int id : 10;
};

struct mapCellPyramid
{
	unsigned int Available : 1;
	unsigned int ID : 4;
	unsigned int Visited : 8;
	unsigned int Spell : 8;
	unsigned int _u1 : 11;
};

struct mapCellRefugeeCamp
{
	int amount;
};

struct mapCellResource
{
	unsigned int Value : 19;
	unsigned int SetupIndex : 12;
	unsigned int HasSetup : 1;
};

struct mapCellScholar
{
	unsigned int Type : 3;
	unsigned int pSkill : 3;
	unsigned int SSkill : 7;
	unsigned int Spell : 10;
	unsigned int _u1 : 9;
};

struct mapCellScroll
{
	unsigned int Type : 8;
	unsigned int _u1 : 11;
	unsigned int ID : 12;
	unsigned int HasSetup : 1;
};

struct mapCellSeaChest
{
	unsigned int Level : 2;
	unsigned int _u1 : 1;
	unsigned int ArtifactID : 10;
	unsigned int _u2 : 19;
};

struct mapCellShipwreckSurvivor
{
	int artifactId;
};

struct mapCellShipyard
{
	unsigned int Owner : 8;
	unsigned int X : 8;
	unsigned int Y : 8;
	unsigned int _u3 : 8;
};

struct mapCellTreasureChest
{
	unsigned int ArtifactID : 10;
	unsigned int HasArtifact : 1;
	unsigned int Bonus : 4;
	unsigned int _u1 : 17;
};

struct mapCellTreeOfKnowledge
{
	unsigned int ID : 5;
	unsigned int Visited : 8;
	unsigned int Type : 2;
	unsigned int _u1 : 17;
};

struct mapCellUniversity
{
	unsigned int _u1 : 5;
	unsigned int Visited : 8;
	unsigned int ID : 12;
	unsigned int _u2 : 7;
};

struct mapCellWagon
{
	unsigned int ResValue : 5;
	unsigned int Visited : 8;
	unsigned int hasBonus : 1;
	unsigned int hasArtifact : 1;
	unsigned int ArtifactID : 10;
	unsigned int ResType : 4;
	unsigned int _u3 : 3;
};

struct mapCellWarriorsTomb
{
	unsigned int HasArt : 1;
	unsigned int _u1 : 4;
	unsigned int Visited : 8;
	unsigned int ArtifactID : 10;
	unsigned int _u2 : 9;
};

struct mapCellWaterMill
{
	unsigned int Gold_0_500_1000 : 5;
	unsigned int Visited : 8;
	unsigned int _u1 : 19;
};

struct mapCellWindMill
{
	unsigned int ResType : 4;
	unsigned int _u1 : 9;
	unsigned int ResValue : 4;
	unsigned int _u2 : 15;
};

struct mapCellWitchHut
{
	unsigned int _u1 : 5;
	int Visited : 8;
	int SSkill : 7;
	unsigned int _u2 : 12;
};

struct ExtraInfoUnion
{
	union
	{
		unsigned int setup;
		mapCellArtifact Artifact;
		mapCellDefaultObject BlackMarket;
		mapCellDefaultObject Boat;
		mapCellCampfire campfire;
		mapCellCorpse corpse;
		mapCellCreatureBank creatureBank;
		mapCellEvent cellEvent;
		mapCellFlotsam flotsam;
		mapCellFountainFortune fountainFortune;
		mapCellDefaultObject garrison;
		mapCellDefaultObject generator;
		mapCellDefaultObject hero;
		mapCellLeanTo leanTo;
		mapCellDefaultObject learningStone;
		mapCellDefaultObject lighthouse;
		mapCellMagicShrine magicShrine;
		mapCellMagicSpring magicSpring;
		mapCellDefaultObject mine;
		mapCellDefaultObject monolith;
		mapCellMonster wanderingCreature;
		mapCellMysticGarden mysticGarden;
		mapCellDefaultObject obelisk;
		mapCellDefaultObject oceanBottle;
		mapCellPandorasBox pandorasBox;
		mapCellDefaultObject prison;
		mapCellPyramid pyramid;
		mapCellDefaultObject questGuard;
		mapCellRefugeeCamp refugeeCamp;
		mapCellResource resource;
		mapCellScholar scholar;
		mapCellScroll spellScroll;
		mapCellSeaChest seaChest;
		mapCellDefaultObject seerHut;
		mapCellShipwreckSurvivor shipwreckSurvivor;
		mapCellShipyard shipyard;
		mapCellDefaultObject signPost;
		mapCellDefaultObject town;
		mapCellTreasureChest treasureChest;
		mapCellTreeOfKnowledge treeKnowledge;
		mapCellUniversity university;
		mapCellWagon wagon;
		mapCellWarriorsTomb warriorsTomb;
		mapCellWaterMill watermill;
		mapCellWindMill windmill;
		mapCellWitchHut witchHut;
	};
};

class NewmapCell
{
public:
	ExtraInfoUnion object_setup;
	int GroundSet : 8;
	unsigned int GroundIndex : 8;
	unsigned int RiverSet : 8;
	int RiverIndex : 8;
	unsigned int RoadSet : 8;
	int RoadIndex : 8;
	unsigned short GroundFlippedHorizontal : 1;
	unsigned short GroundFlippedVertical : 1;
	unsigned short RiverFlippedHorizontal : 1;
	unsigned short RiverFlippedVertical : 1;
	unsigned short RoadFlippedHorizontal : 1;
	unsigned short RoadFlippedVertical : 1;
	unsigned short Passable : 1;
	unsigned short Animated : 1;
	unsigned short IsBlocked : 1;
	unsigned short IsBeachBorder : 1;
	unsigned short unused_bit : 1;
	unsigned short can_build_ship : 1;
	unsigned short is_trigger : 1;
	std::vector<int> ObjectCellList;
	int type;
	short objectIndex;
	short object_type_index;
};

class NewfullMap
{
public:
	std::vector<int> ObjectTypes;
	std::vector<int> Objects;
	std::vector<int> Sprites;
	std::vector<int> CustomTreasureList;
	std::vector<int> CustomMonsterList;
	std::vector<int> BlackBoxList;
	std::vector<int> SeerHutList;
	std::vector<int> QuestGuardList;
	std::vector<int> TimedEventList;
	std::vector<int> TownEventList;
	std::vector<int> PlaceHolderList;
	std::vector<int> QuestList;
	std::vector<int> RandomDwellingList;
	NewmapCell *cellData;
	int Size;
	unsigned char HasTwoLevels;
	std::vector<int> object_templates[232];
};

class AI
{
public:
	int resource_expected_count[7];
	int turnProductionResource[7];
	double resource_value[7];
	int average_resource_value;
	float turnValueOfAvgArtifact;
};

class type_point
{
public:
   short x : 10;
   short y : 10;
   short z : 4;

   inline bool is_valid()
   {
	  return CALL_1(bool, __thiscall, 0x4B1090, this);
   }
};

enum TAdventureObjectType : int
{
	// RoE
	NOTHING = 0,
	ALTAR_OF_SACRIFICE = 2,
	ANCHOR_POINT = 3,
	ARENA = 4,
	ARTIFACT = 5,
	BLACK_BOX = 6,
	BLACK_MARKET = 7,
	BOAT = 8,
	BORDER_GUARD = 9,
	BORDER_TENT = 10,
	BUOY = 11,
	CAMPFIRE = 12,
	CARTOGRAPHER = 13,
	SWAN_POND = 14,					// CLOVER_FIELD = 14,
	COVER_OF_DARKNESS = 15,
	CREATURE_BANK = 16,
	CREATURE_GENERATOR_1 = 17,
	CREATURE_GENERATOR_2 = 18,		// Not in the dump
	CREATURE_GENERATOR_3 = 19,		// Not in the dump
	CREATURE_GENERATOR_4 = 20,
	CURSED_GROUND = 21,
	DEAD_GUY = 22,
	DEFENSE_TOWER = 23,
	DERELICT_SHIP = 24,
	DRAGON_CITY = 25,
	EVENT = 26,
	EYE_OF_MAGI = 27,
	FAERIE_RING = 28,
	FLOTSAM = 29,
	FOUNTAIN_OF_FORTUNE = 30,
	FOUNTAIN_OF_YOUTH = 31,
	GARDEN_OF_REVELATION = 32,
	GARRISON = 33,
	HERO = 34,
	HILL_FORT = 35,
	HOLY_GRAIL = 36,
	HUT_OF_MAGI = 37,
	IDOL_OF_FORTUNE = 38,
	LEAN_TO = 39,
	DECORATIVE = 40,				// Not in the dump
	LIBRARY = 41,
	LIGHTHOUSE = 42,
	LITH_ONEWAY_ENTRANCE = 43,
	LITH_ONEWAY_EXIT = 44,
	LITH_TWOWAY = 45,
	MAGIC_PLAINS = 46,
	MAGIC_SCHOOL = 47,
	MAGIC_SPRING = 48,
	MAGIC_WELL = 49,
	MARKET_OF_TIME = 50,
	MERC_CAMP = 51,
	MERMAID = 52,
	MINE = 53,
	MONSTER = 54,
	MYSTICAL_GARDEN = 55,
	OASIS = 56,
	OBELISK = 57,
	OBSERVATORY = 58,
	OCEAN_BOTTLE = 59,
	PILLAR_OF_FIRE = 60,
	POWER_SCHOOL = 61,
	PRISON = 62,
	PYRAMID = 63,
	RALLY_FLAG = 64,
	RANDOM_ARTIFACT = 65,
	RANDOM_ARTIFACT_1 = 66,
	RANDOM_ARTIFACT_2 = 67,
	RANDOM_ARTIFACT_3 = 68,
	RANDOM_ARTIFACT_4 = 69,
	RANDOM_HERO = 70,
	RANDOM_MONSTER = 71,
	RANDOM_MONSTER_1 = 72,
	RANDOM_MONSTER_2 = 73,
	RANDOM_MONSTER_3 = 74,
	RANDOM_MONSTER_4 = 75,
	RANDOM_RESOURCE = 76,
	RANDOM_TOWN = 77,
	REFUGEE_CAMP = 78,
	RESOURCE = 79,
	SANCTUARY = 80,
	SCHOLAR = 81,
	SEA_CHEST = 82,
	SEER = 83,
	SEPULCHER = 84,
	SHIPWRECK = 85,
	SHIPWRECK_SURVIVOR = 86,
	SHIPYARD = 87,
	SHRINE1 = 88,
	SHRINE2 = 89,
	SHRINE3 = 90,
	SIGN = 91,
	SIREN = 92,
	SPELL_SCROLL = 93,
	STABLES = 94,
	TAVERN = 95,
	TEMPLE = 96,
	THIEVES_DEN = 97,
	TOWN = 98,
	TRADING_POST = 99,
	TRAINIG_GROUNDS = 100,
	TREASURE_CHEST = 101,
	TREE_OF_KNOWLEDGE = 102,
	UNDERGROUND_GATE = 103,
	UNIVERSITY = 104,
	WAGON = 105,
	WAR_MACHINE_FACTORY = 106,
	WAR_SCHOOL = 107,
	WARRIOR_TOMB = 108,
	WATER_WHEEL = 109,
	WATERING_HOLE = 110,
	WHIRLPOOL = 111,
	WINDMILL = 112,
	WITCH_HUT = 113,
	TERRAIN_BRUSH = 114,
	TERRAIN_BUSH = 115,
	TERRAIN_CACTUS = 116,
	TERRAIN_CANYON = 117,
	TERRAIN_CRATER = 118,
	TERRAIN_DEAD_VEGETATION = 119,
	TERRAIN_FLOWER = 120,
	TERRAIN_FROZEN_LAKE = 121,
	TERRAIN_HEDGE = 122,
	TERRAIN_HILL = 123,
	TERRAIN_HOLE = 124,
	TERRAIN_KELP = 125,
	TERRAIN_LAKE = 126,
	TERRAIN_LAVA_FLOW = 127,
	TERRAIN_LAVA_LAKE = 128,
	TERRAIN_MUSHROOM = 129,
	TERRAIN_LOG = 130,
	TERRAIN_MANDRAKE = 131,
	TERRAIN_MOSS = 132,
	TERRAIN_MOUND = 133,
	TERRAIN_MOUNTAIN = 134,
	TERRAIN_OAK_TREE = 135,
	TERRAIN_OUTCROPPING = 136,
	TERRAIN_PINE_TREE = 137,
	TERRAIN_PLANT = 138,
	TERRAIN_BLANK_1 = 139,				// TERRAIN_RIVER_1 = 139,
	TERRAIN_BLANK_2 = 140,				// TERRAIN_RIVER_2 = 140,
	TERRAIN_BLANK_3 = 141,				// TERRAIN_RIVER_3 = 141,
	TERRAIN_BLANK_4 = 142,				// TERRAIN_RIVER_4 = 142,
	TERRAIN_RIVER_DELTA = 143,
	TERRAIN_BLANK_5 = 144,				// TERRAIN_ROAD_1 = 144,
	TERRAIN_BLANK_6 = 145,				// TERRAIN_ROAD_2 = 145,
	TERRAIN_BLANK_7 = 146,				// TERRAIN_ROAD_3 = 146,
	TERRAIN_ROCK = 147,
	TERRAIN_SAND_DUNE = 148,
	TERRAIN_SAND_PIT = 149,
	TERRAIN_SHRUB = 150,
	TERRAIN_SKULL = 151,
	TERRAIN_STALAGMITE = 152,
	TERRAIN_STUMP = 153,
	TERRAIN_TAR_PIT = 154,
	TERRAIN_TREE = 155,
	TERRAIN_VINE = 156,
	TERRAIN_VOLCANIC_TENT = 157,
	TERRAIN_VOLCANO = 158,
	TERRAIN_WILLOW_TREE = 159,
	TERRAIN_YUCCA_TREE = 160,
	TERRAIN_REEF = 161,
	RANDOM_MONSTER_5 = 162,
	RANDOM_MONSTER_6 = 163,
	RANDOM_MONSTER_7 = 164,
	MAX_EVENT_TYPE = 165,
	const_first_terrain_object = 114,
	// AB + SoD
	TERRAIN_BRUSH_2 = 165,
	TERRAIN_BUSH_2 = 166,
	TERRAIN_CACTUS_2 = 167,
	TERRAIN_CANYON_2 = 168,
	TERRAIN_CRATER_2 = 169,
	TERRAIN_DEAD_VEGETATION_2 = 170,
	TERRAIN_FLOWER_2 = 171,
	TERRAIN_FROZEN_LAKE_2 = 172,
	TERRAIN_HEDGE_2 = 173,
	TERRAIN_HILL_2 = 174,
	TERRAIN_HOLE_2 = 175,
	TERRAIN_KELP_2 = 176,
	TERRAIN_LAKE_2 = 177,
	TERRAIN_LAVA_FLOW_2 = 178,
	TERRAIN_LAVA_LAKE_2 = 179,
	TERRAIN_MUSHROOM_2 = 180,
	TERRAIN_LOG_2 = 181,
	TERRAIN_MANDRAKE_2 = 182,
	TERRAIN_MOSS_2 = 183,
	TERRAIN_MOUND_2 = 184,
	TERRAIN_MOUNTAIN_2 = 185,
	TERRAIN_OAK_TREE_2 = 186,
	TERRAIN_OUTCROPPING_2 = 187,
	TERRAIN_PINE_TREE_2 = 188,
	TERRAIN_PLANT_2 = 189,
	TERRAIN_RIVER_DELTA_2 = 190,
	TERRAIN_ROCK_2 = 191,
	TERRAIN_SAND_DUNE_2 = 192,
	TERRAIN_SAND_PIT_2 = 193,
	TERRAIN_SHRUB_2 = 194,
	TERRAIN_SKULL_2 = 195,
	TERRAIN_STALAGMITE_2 = 196,
	TERRAIN_STUMP_2 = 197,
	TERRAIN_TAR_PIT_2 = 198,
	TERRAIN_TREE_2 = 199,
	TERRAIN_VINE_2 = 200,
	TERRAIN_VOLCANIC_TENT_2 = 201,
	TERRAIN_VOLCANO_2 = 202,
	TERRAIN_WILLOW_TREE_2 = 203,
	TERRAIN_YUCCA_TREE_2 = 204,
	TERRAIN_REEF_2 = 205,
	TERRAIN_DESERT_HILL = 206,
	TERRAIN_DIRT_HILL = 207,
	TERRAIN_GRASS_HILL = 208,
	TERRAIN_ROUGH_HILL = 209,
	TERRAIN_UNDERGROUND_ROCK = 210,
	TERRAIN_SWAMP_FOLIAGE = 211,
	BORDER_GATE = 212,
	HERO_PLACEHOLDER = 214,
	QUEST_GUARD = 215,
	RANDOM_DWELLING = 216,
	RANDOM_DWELLING_LVL = 217,
	RANDOM_DWELLING_FACTION = 218,
	GARRISON_2 = 219,
	ABANDONED_MINE = 220,
	TRADING_POST_SNOW = 221,
	CLOVER_FIELD = 222,
	CURSED_GROUND_2 = 223,
	EVIL_FOG = 224,
	FAVORABLE_WINDS = 225,
	FIERY_FIELDS = 226,
	HOLY_GROUNDS = 227,
	LUCID_POOLS = 228,
	MAGIC_CLOUDS = 229,
	MAGIC_PLAINS_2 = 230,
	ROCKLANDS = 231
};

class armyGroup
{
public:
   TCreatureType armies[7];
   int numTroops[7];
};

class type_obscuring_object
{
public:
   short mapX;								 // 0x000
   short mapY;								 // 0x002
   short mapZ;								 // 0x004
   bool valid;								 // 0x006   
   type_point obscured_location;			 // 0x008
   TAdventureObjectType type;				 // 0x00C
   bool was_trigger;						 // 0x010
   unsigned long extra_info;				 // 0x014
											 // 0x018
};

#pragma pack(push, 1)
class hero : public type_obscuring_object
{
public:
   short mana;								 // 0x018
   THeroID id;								 // 0x01A
   int map_editor_id;						 // 0x01E
   char playerOwner;						 // 0x022
   char name[13];							 // 0x023
   THeroClass hero_class;					 // 0x030
   unsigned char portrait;					 // 0x034
   int targetX;								 // 0x035
   int targetY;								 // 0x039
   short targetZ;							 // 0x03D
   short last_magic_school_level;			 // 0x03F
   unsigned short target_distance;			 // 0x041
   unsigned char target_is_critical;		 // 0x043
   unsigned char patrolX;					 // 0x044
   unsigned char patrolY;					 // 0x045
   char patrolRadius;						 // 0x046
   unsigned char facing;					 // 0x047
   
   enum Formation : int {
	  GroupedFormation = 1,
	  PlacementFormation = 2
   };

   unsigned char formation;					 // 0x048
   int maxMobility;							 // 0x049
   int currMobility;						 // 0x04D
   int experience;							 // 0x051
   short Level;								 // 0x055
   unsigned long TrainingGroundFlags;		 // 0x057
   unsigned long DefenseTowerFlags;			 // 0x05B
   unsigned long GardenOfRevelationFlags;	 // 0x05F
   unsigned long MercCampFlags;				 // 0x063
   unsigned long PowerSchoolFlags;			 // 0x067
   unsigned long TreeOfKnowledgeFlags;		 // 0x06B
   unsigned long LibraryFlags;				 // 0x06F
   unsigned long ArenaFlags;				 // 0x073
   unsigned long MagicSchoolFlags;			 // 0x077
   unsigned long WarSchoolFlags;			 // 0x07B
   unsigned long UniversityFlags;			 // 0x07F
   unsigned long Shrine1Flags;				 // 0x083
   unsigned long Shrine2Flags;				 // 0x087
   unsigned long Shrine3Flags;				 // 0x08B
   unsigned char iLevelSeed;				 // 0x08F
   unsigned char lastWisdom;				 // 0x090
   armyGroup heroArmy;						 // 0x091
   char SSLevel[28];						 // 0x0C9
   unsigned char SSOrder[28];				 // 0x0E5
   int numSSs;								 // 0x101
   unsigned long flags;						 // 0x105
   float turnExperienceToRVRatio;			 // 0x109
   char dWalkSpellsCast;					 // 0x10D
   TSkillMastery disguiseLevel;				 // 0x10E
   TSkillMastery flightLevel;				 // 0x112
   TSkillMastery waterWalkPower;			 //	0x116
   char moraleBonus;						 // 0x11A
   char luckBonus;							 // 0x11B
   bool IsSleeping;							 // 0x11C
   long bounty;								 // 0x11D								
   std::bitset<48> TownSpecialGrantedMask;	 // 0x121
   TSkillMastery visionsPower;				 // 0x129
   type_artifact equipped[19];				 // 0x12D
   bool locked_slot[15];					 // 0x1C6
   type_artifact backpack[64];				 // 0x1D4
   char backpack_count;						 // 0x3D4
   TSex sex;								 // 0x3D5
   bool IsBiographyCustomized;				 // 0x3D9
   stdString customBiography;				 // 0x3DA
   char in_spellbook[140];					 // 0x3EA
   //bool in_spellbook[70];					 // 0x3EA
   //bool available_spells[70];				 // 0x430
   char stats[4];							 // 0x476
   float aggression;						 // 0x47A
   long value_of_power;						 // 0x47E
   long value_of_duration;					 // 0x482
   long value_of_knowledge;					 // 0x486
   long value_of_spring;					 // 0x48A
   long value_of_well;						 // 0x48E
											 // 0x492

	inline int get_spell_level(SpellID spell, int land_modifier) {
	   return CALL_3(int, __thiscall, 0x4E52F0, this, spell, land_modifier); 
	}
	inline int GetManaCost(int iWhichSpell, armyGroup* const enemy, int land_modifier) {
	   return CALL_4(int, __thiscall, 0x4E54B0, this, iWhichSpell, enemy, land_modifier);
	}
	inline void UseSpell(int cost) {
	   CALL_2(void, __thiscall, 0x4D9540, this, cost);
	}
	inline int GetHeroSpellBonus(SpellID spell_id, int target_level, int value) {
	   return CALL_4(int, __thiscall, 0x4E6260, this, spell_id, target_level, value);
	}
	void AddSpell(int whichSpell);
	void UpdateSpellsFromArtifacts();
	int getBestMagicSchoolForSpell(SpellID spell);
	inline TAdventureObjectType get_special_terrain() {
	   return CALL_1(TAdventureObjectType, __thiscall, 0x4E5210, this);
	}
};
#pragma pack(pop)

class HeroExtra
{
public:
	char Owner;						// 0x000
	int number;						// 0x004
	THeroID id;						// 0x008
	char bCustomName;				// 0x00C
	char Name[13];					// 0x00D
	int Experience;					// 0x01C
	char bCustomPortraitNumber;		// 0x020
	unsigned char PortraitNumber;	// 0x021
	char bCustomSecondarySkills;	// 0x022
	int NumSecondarySkills;			// 0x024
	char secondarySkill[8];			// 0x028
	char secondarySkillLevel[8];	// 0x030	
	char bCustomArmies;				// 0x038
	TCreatureType armies[7];		// 0x039
	short numTroops[7];				// 0x058
	char GroupFormation;			// 0x066
	char bCustomArtifacts;			// 0x067
	type_artifact artifacts[19];	// 0x068
	type_artifact backpack[64];		// 0x100
	unsigned char numInBackpack;	// 0x300
	type_point location;			// 0x301
	char PatrolRadius;				// 0x305
	char bCustomBiography;			// 0x306
	stdString sBiography;			// 0x308
	int sex;						// 0x318
	char bCustomSpells;				// 0x31C
	std::bitset<70> customSpells;	// 0x320
	char bCustomPrimarySkills;		// 0x32C
	char primarySkills[4];			// 0x32D
};

class TCreatureTypeTraits
{
public:
   TTownType townType;				// 0x000
   int level;						// 0x004
   char* cSamplePrefix;				// 0x008
   char* m_sprite_name;				// 0x00C
   unsigned long attributes;		// 0x010
   char* m_name;					// 0x014
   char* m_plural_name;				// 0x018
   char* special_ability;			// 0x01C
   int cost[7];						// 0x020
   int baseFightValue;				// 0x03C
   int AI_value;					// 0x040 
   int growthRate;					// 0x044
   short horde_growth_rate;			// 0x048
   int hitPoints;					// 0x04C
   int speed;						// 0x050
   int attackSkill;					// 0x054
   int defenseSkill;				// 0x058
   int damageLowBound;				// 0x05C
   int damageHighBound;				// 0x060
   int numShots;					// 0x064
   SpellID spell;					// 0x068 
   int wanderingLow;				// 0x06C
   int wanderingHigh;				// 0x070
									// 0x074
};

class SMonFrameInfo
{
public:
   short iMissileOffset[3][2];		// 0x000
   float fArrowAngle[12];			// 0x00C
   int iExtraNumTroopsXOffset;		// 0x03C
   int iAttackFrames;				// 0x040
   int iFidgetFrequency;			// 0x044
   int iWalkCycleTime;				// 0x048
   int iAttackStartCycleTime;		// 0x04C
   int iFlightPixelSpan;			// 0x050
									// 0x054
};

class army
{
public:
   enum : int {
	  OFFSET_X = 196,
	  OFFSET_Y = 267
   };

   enum TSampleID : int {
	  WALK_SAMPLE = 0,
	  ATTACK_SAMPLE = 1,
	  WINCE_SAMPLE = 2,
	  SHOOT_SAMPLE = 3,
	  DIE_SAMPLE = 4,
	  DEFEND_SAMPLE = 5,
	  PRE_WALK_SAMPLE = 6,
	  POST_WALK_SAMPLE = 7,
	  MAX_SAMPLES = 8
   };

   bool bShowAttackFrames;			   // 0x000
   bool bShowRangeFrames;			   // 0x001
   char iShowAttackFrameType;		   // 0x002
   char iNextFrameType;				   // 0x003
   char iRemainingFramesToPlay;		   // 0x004
   int iDrawPriority;				   // 0x008
   bool bShowTroopCount;			   // 0x00C
   int groupToAttack;				   // 0x010
   int indexToAttack;				   // 0x014
   int attackLimit;					   // 0x018
   int targetCellIndex;				   // 0x01C
   bool bShowPowEffect;				   // 0x020
   int iMirrorSourceIndex;			   // 0x024
   int iMirrorDestIndex;			   // 0x028
   int iRoundsLeftBeforeVanish;		   // 0x02C
   bool IsMoving;					   // 0x030
   bool LetsPretendImNotHere;		   // 0x031
   TCreatureType armyType;			   // 0x034
   int gridIndex;					   // 0x038
   int currFrameType;				   // 0x03C
   int currFrameIndex;				   // 0x040
   int facing;						   // 0x044
   int walkDirection;				   // 0x048
   int numTroops;					   // 0x04C
   int numTroopsToShowOverride;		   // 0x050
   int numTroopsBattleResurrected;	   // 0x054
   int residualDamage;				   // 0x058
   int origPos;						   // 0x05C
   int origNumTroops;				   // 0x060
   int origSpeed;					   // 0x064
   int origWalkCycleTime;			   // 0x068
   int origHitPoints;				   // 0x06C
   int iLuckStatus;					   // 0x070
   TCreatureTypeTraits sMonInfo;	   // 0x074
   bool show_fire_shield;			   // 0x0E8
   bool bSomeUnitsDamaged;			   // 0x0E9
   bool bAllUnitsKilled;			   // 0x0EA
   SpellID iPostPowSpellToCast;		   // 0x0EC
   bool hitByCreature;				   // 0x0F0
   int group;						   // 0x0F4
   int index;						   // 0x0F8
   unsigned long iLastFidgetTime;	   // 0x0FC
   int ySpecialMod;					   // 0x100
   int xSpecialMod;					   // 0x104
   int bPowSequenceComplete;		   // 0x108
   char* yModify;					   // 0x10C
   SMonFrameInfo sMonFrameInfo;		   // 0x110
   CSprite* stdIcon;				   // 0x164
   CSprite* missileIcon;			   // 0x168
   int image_height;				   // 0x16C
   unsigned int armySample[8];		   // 0x170
   long expected_move_order;		   // 0x190
   int numSpellInfluences;			   // 0x194
   int spellInfluence[162];			   // 0x198
   //int spellInfluence[81];		   // 0x198
   //TSkillMastery spell_level[81];	   // 0x2DC	
   stdDeque SpellInfluenceQueue;	   // 0x420
   float PaletteEffect;				   // 0x450
   int retaliationCount;			   // 0x454
   long blessFactor;				   // 0x458
   long curseFactor;				   // 0x45C
   int antiMagicSpellLevel;			   // 0x460
   int bloodlustBonus;				   // 0x464
   int precisionBonus;				   // 0x468
   int weaknessPenalty;				   // 0x46C
   int toughskinBonus;				   // 0x470
   int disruptiverayPenalty;		   // 0x474
   int prayerBonus;					   // 0x478
   int mirthBonus;					   // 0x47C
   int sorrowPenalty;				   // 0x480
   int fortuneBonus;				   // 0x484
   int misfortunePenalty;			   // 0x488
   int slayerLevel;					   // 0x48C
   int joustBonus;					   // 0x490
   int counterstrokeBonus;			   // 0x494
   float frenzyAdjust;				   // 0x498
   float blindFactor;				   // 0x49C
   float fire_shield_strength;		   // 0x4A0
   float poison_penalty;			   // 0x4A4
   float protectionFromAirFactor;	   // 0x4A8
   float protectionFromFireFactor;	   // 0x4AC
   float protectionFromWaterFactor;	   // 0x4B0
   float protectionFromEarthFactor;	   // 0x4B4
   float shieldDamageFactor;		   // 0x4B8
   float airShieldDamageFactor;		   // 0x4BC
   bool residualBlindness;			   // 0x4C0
   bool residualParalyze;			   // 0x4C1
   TSkillMastery forgetfulness_level;  // 0x4C4
   float slowPenalty;				   // 0x4C8
   int tailwindBonus;				   // 0x4CC
   int diseaseDefensePenalty;		   // 0x4D0
   int diseaseAttackPenalty;		   // 0x4D4
   bool OnNativeTerrain;			   // 0x4D8
   int DefendBonus;					   // 0x4DC
   int faerieDragonSpell;			   // 0x4E0
   long backlash_chance;			   // 0x4E4
   int iMorale;						   // 0x4E8
   int iLuck;						   // 0x4EC
   bool reset_this_round;			   // 0x4F0
   bool is_area_effect_target;		   // 0x4F1
   exe_vector<army*> bound_armies;	   // 0x4F4
   exe_vector<army*> binder;		   // 0x504
   exe_vector<army*> aura_clients;	   // 0x514
   exe_vector<army*> aura_sources;	   // 0x524
   long AI_expected_damage;			   // 0x534
   army* AI_target;					   // 0x538
   long AI_target_value;			   // 0x53C
   long AI_target_distance;			   // 0x540
   long AI_possible_targets;		   // 0x544
									   // 0x548
   
   inline army* copyConstructor(army* const Army) {
	  return CALL_2(army*, __thiscall, 0x437650, this, Army);
   }
   ~army() {
	  CALL_1(void, __thiscall, 0x43D120, this);
   }
   inline int get_adjusted_attack(army* const enemy, bool ranged_attack) const {
	  return CALL_3(int, __thiscall, 0x442130, this, enemy, ranged_attack);
   }
   int get_adjusted_defense(army* const enemy, bool frenzy_included);
   inline void CancelIndividualSpell(int SpellID) {
	  CALL_2(void, __thiscall, 0x444230, this, SpellID);
   }
   inline int get_total_hit_points(bool simulated = false) const {
	  return CALL_2(int, __thiscall, 0x442DA0, this, simulated);
   }
   inline int currentHealth() {
	  return this->numTroops * this->sMonInfo.hitPoints - this->residualDamage;
   }
   inline int fightValue() {
	  return this->sMonInfo.baseFightValue * this->numTroops;
   }
   bool can_attack(army* target);
   inline bool can_shoot(army* const excluded = NULL) const {
	  return CALL_2(bool, __thiscall, 0x442610, this, excluded);
   }
   double get_average_damage() const;
   inline int get_average_damage(army* const enemy, bool ranged_attack, long amount, bool limit_damage = true, long distance = 0) const {
	  return CALL_6(int, __thiscall, 0x4424A0, this, enemy, ranged_attack, amount, limit_damage, distance);
   }
   inline int get_total_combat_value(long lowest_attack, long lowest_defense) {
	  return CALL_3(int, __thiscall, 0x442B80, this, lowest_attack, lowest_defense);
   }
   inline int get_loss_combat_value(long lowest_attack, long lowest_defense, bool ranged, long damage, bool kills_only) {
	  return CALL_6(int, __thiscall, 0x442CF0, this, lowest_attack, lowest_defense, ranged, damage, kills_only);
   }
   inline int get_AI_target_time(long speed) const {
	  return CALL_2(int, __thiscall, 0x4488F0, this, speed);
   }
   inline int Damage(int damage) {
	  return CALL_2(int, __thiscall, 0x443DB0, this, damage);
   }
   inline void SetSpellInfluence(enum SpellID spell, int spellPower, int newSpellMastery, const hero* Hero) {
	  CALL_5(void, __thiscall, 0x444610, this, spell, spellPower, newSpellMastery, Hero);
   }
   int GetSpeed();
   int get_resurrection_size(army* const target) const;
};

static_assert(offsetof(army, bound_armies) == 0x4F4, "army::bound_armies ABI mismatch");
static_assert(offsetof(army, aura_sources) == 0x524, "army::aura_sources ABI mismatch");
static_assert(offsetof(army, AI_expected_damage) == 0x534, "army::AI_expected_damage ABI mismatch");
static_assert(offsetof(army, AI_target) == 0x538, "army::AI_target ABI mismatch");
static_assert(sizeof(army) == 0x548, "army ABI mismatch");

class CMapHeaderData
{
public:
	int iVersion;								// 0x00
	unsigned char IsPlayable;					// 0x04
	unsigned char iDifficulty;					// 0x05
	unsigned char numPlayers;					// 0x06
	unsigned char minNumHumanPlayers;			// 0x07
	unsigned char maxNumHumanPlayers;			// 0x08
	unsigned char lastTownNameAssigned;			// 0x09
	unsigned char mapHasNotBeenSaved;			// 0x0A
	char max_hero_level;						// 0x0B
	char numTeams;								// 0x0C
	char teamInfo[8];							// 0x0D
	int Size;									// 0x18
	unsigned char HasTwoLayers;					// 0x1C
	std::vector<int> place_holder_hero_ids;
	VictoryConditionStruct victory_condition;
	LossConditionStruct loss_condition;
	
	struct TPlayerSlotAttributes
	{
		unsigned char CanBeHuman;
		unsigned char CanBeComputer;
		int AIStrategy;
		unsigned char legal_alignments;
		unsigned char HasRandomAlignment;
		unsigned char GenerateHero;
		unsigned char has_main_town;
		int main_town_type;
		type_point CastleLoc;
		char hasRandomHero;
		THeroID nonRandomHeroId;
		THeroID nonRandomHeroCustomPortrait;
		char nonRandomHeroCustomName[12];
		int undefined_place_holder_count;
		std::vector<int> player_heroes;
		int field_unknown;
	} PlayerSlotAttributes[8];
		
	std::vector<int> free_heroes;
};

class NewSMapHeader : public CMapHeaderData
{
public:
	stdString Name;
	stdString Description;
	std::bitset<156> hero_available;
};

class playerData
{
public:
	char color;
	char numHeroes;
	int currHero;
	THeroID heroes[8];
	THeroID recruits[2];
	unsigned char startingNumHeroes;
	int personality;
	type_point puzzle_guess;
	char extraPuzzlePieces;
	char iDeathCountDown;
	char numTowns;
	char currTown;
	char towns[72];
	unsigned char placement_help_enabled;
	std::vector<int> shipyards;
	int resources[7];
	unsigned long MysticalGardenFlags;
	unsigned long MagicSpringFlags;
	unsigned long DeadGuyFlags;
	unsigned long LeanToFlags;
	unsigned long dpid;
	char cName[21];
	unsigned char isLocal;
	unsigned char isHuman;
	int quickCombat;
	int constructed_combo_arts_bitset;
	AI ai;
	float artifact_value;
};

class game
{
public:
	heroWindow *newGameWin;
	unsigned char spellAllocInfo[70];
	unsigned char spellDisabledInfo[70];
	LPCRITICAL_SECTION bink_crit_section;
	std::vector<int> townExtraPool;
	HeroExtra heroExtraPool[156];
	int difficultyRating;
	SCampaign sCampaign;
	char bNewCampaignStarted;
	char cGameFilename[351];
	char numPlayers;
	char numDeadPlayers;
	char playerDead[8];
	unsigned short day;
	unsigned short week;
	unsigned short month;
	char cUniqueSystemID[32];
	TArtifact marketArtifacts[7];
	std::vector<int> BlackMarkets;
	short ultimateX;
	short ultimateY;
	unsigned char ultimateZ;
	unsigned char ultimateRadius;
	unsigned char ultimateValid;
	int iGameType;
	char bIsCheater;
	unsigned char is_tutorial;
	SGameSetupOptions sSetup;
	NewSMapHeader sMapHeader;
	NewfullMap worldMap;
	playerData player[8];
	std::vector<int> townPool;
	hero heroPool[156];
	char heroAllocInfo[156];
	int hero_available_for_player_bitset[156];
	char artifactAllocInfo[144];
	char reservedArtifactInfo[144];
	unsigned char InfoFlags[32];
	unsigned char GuardFlags[8];
	unsigned short cartographerMask[3];
	unsigned char cartographerFlags[3];
	std::vector<int> signPool;
	std::vector<int> minePool;
	std::vector<int> generatorPool;
	std::vector<int> garrisonPool;
	std::vector<int> boatPool;
	std::vector<int> university_pool;
	std::vector<int> creature_banks;
	char numObelisks;
	char obeliskPool[48];
	char cCurRumour[300];
	unsigned char rumourAllocInfo[256];
	std::vector<int> MapRumours;
	unsigned char sec_skill_skill_disabled[28];
	heroWindow *armyWindow;
	std::vector<int> two_way_liths[8];
	std::vector<int> lith_exits[8];
	std::vector<int> whirlpools;
	std::vector<int> underground_gates;
	std::vector<int> underground_gate_exits;
	std::vector<int> recorded_events;
	std::vector<int> load_map_monster_link;
};

// ---------------------------------------------------------------

struct H3Bitfield
{
	struct reference
	{
		H3Bitfield* m_bitfield;
		UINT32      m_position;
	};

	UINT m_bf;
};

struct H3CustomHeroData
{
	int heroId;
	stdString name;
};

struct H3PlayerAttributes
{
	bool humanPlayable;
	bool computerPlayable;
	int aiBehaviour;
	unsigned short availableFactionsBitset;
	bool ownsRandomTown;
	bool generateHeroAtMainTown;
	bool hasMainTown;
	int generateHero;
	type_point mainTownPosition;
	bool hasRandomHero;
	int mainHeroId;
	int mainHeroCustomPicture;
	char customName[12];
	int powerPlaceholder;
	std::vector<H3CustomHeroData> heroesData;
};

struct H3MapInfo
{
	int mapVersion;
	bool hasPlayers;
	char mapDifficulty;
	char computerPlayableCount;
	char humanOnlyCount;
	char humanOnly_8;
	char _f_9;
	char _f_A;
	char maxHeroLevel;
	bool hasTeams;
	char playerTeam[8];
	int mapDimension;
	bool hasUnderground;
	std::vector<char> _f_20;
	VictoryConditionStruct victory;
	LossConditionStruct loss;
	H3PlayerAttributes playerAttributes[8];
	std::vector<int> playerHeroes;
	stdString mapName;
	stdString mapDescription;
	H3Bitfield expansionHeroes[5];
};

struct H3PlayersInfo
{
	char _f_000[8];
	char handicap[8];
	int townType[8];
	char playerType[8];
	char difficulty;
	char filename[251];
	char saveDirectory[100];
	bool isPlayable[8];
	char _f_1A0[3];
	char turnDuration;
	int heroMaybe[8];
	char _f_1C4[8];
};

struct H3ScenarioMapInformation : H3MapInfo
{
	H3PlayersInfo playersInfo;
	short _f_4D0;
	char _f_4D2[30];
	char heroOwner[156];
	char mapNameArray[61];
	char mapDescriptionArray[300];
	FILETIME fileTime;
	char gap700[8];
	int _f_708;
	char _f_70C[4];
	H3MapInfo mapinfo;
	H3PlayersInfo playersinfo;
	char _f_BE0[64];
	std::vector<std::vector<hero>> heroes;
	char _f_C30[16];
	std::vector<char> _f_C40;
	std::vector<char> _f_C50;
	stdString _f_C60;
	char _f_C70[8];
	bool isPlayerAbsent[8];
	int _f_C80[8];
	int _f_CA0;
};

struct H3ScenarioPlayer
{
	int player;
	char name[24];
	int* gameVersionPtr;
	int gameVersion;
	int town;
	int heroesCount;
	int heroes[16];
	int bonusType;
	int player2;
	int _f_74;
	char _f_78;
};

/*class TSingleSelectionWindow : public CAdvPopup 
{
public:
	char my_index;
	char pos_copy;
	char hero_pos[8];
	unsigned char loadMode;
	textWidget* human;
	textWidget* text;
	widget* handicap;
	widget* townL;
	widget* townR;
	widget* heroL;
	widget* heroR;
	widget* resL;
	widget* resR;
	widget* name;
	widget* flag;
	widget* face;
	widget* town;
	widget* bonus;
	textEntryWidget* nameEdit;
	unsigned long freeBlocks;
	unsigned long neededBlocks;
	int SavePart;
	char tempFilename[13];
	long tempBufStart;
	int SelectedVMPort;

	enum EWidgetIDs
	{
	};

	unsigned long clickTime;
	GameSelectionHeadersStruct* SelectionHeaders;
	int num_mapFiles;
	int max_maps;
	int* mapFilter;
	int mapsInFilter;
	unsigned char loadGameMode;
	unsigned char saveGameMode;
	int textIndex;
	CSprite* VictoryIcon;
	CSprite* LossIcon;
	CSprite* TownPix;
	CSprite* Resource;
	CSprite* heroSpecificAbility;
	Bitmap816* GoldBox;
	Bitmap816* Flags[8];
	Bitmap816* Panels[8];
	Bitmap816* HeroPix[156];
	Bitmap816* randomTownQuestion;
	Bitmap816* randomHeroQuestion;
	Bitmap816* randomTown;
	Bitmap816* randomHero;
	Bitmap816* noDice;
	Bitmap816* noHero;
	int sortDirection;
	int currentIndex;
	int currentMap;
	int durationIndex;
	unsigned char inAdvancedOptions;
	unsigned char inScenarioOptions;
	textEntryWidget* saveGameEdit;
	unsigned char mode;
	CNewPlayerUpdateMan* pNewPlayerUpdateMan;
	CNetPlayerHandler netPlayerHandler;
	unsigned char receivedMaps;
	slider* chatSlider;
	slider* fileSlider;
	slider* durationSlider;
	slider* nameSlider;
	textWidget* chatWidget;
	textWidget* nameList1;
	textWidget* nameList2;
	unsigned char mapChanged;
	unsigned char readingMaps;
	CChatEdit* chatEdit;
	int sortWhich;
	int filterSize;
	unsigned char scenarioOptionsStarted;
	unsigned char chatShowing;
	textButton* chatToggle;
	unsigned char receivingMaps;
	CSaveScreen* flagBack;
	char gameVersion[20];
	CSingleSelectionNetMsgHandler netMsgHandler;

	enum EOtherWidgetIDs
	{
	};

	enum
	{
		NWIDGETS = 211
	};
};*/

class TSingleSelectionWindow : public CAdvPopup
{
public:
	unsigned long clickTime;
	unsigned char isCampaign;
	unsigned char loadGameMode;
	unsigned char saveGameMode;
	int textIndex;
	CSprite* scselcDef;
	CSprite* VictoryIcon;
	CSprite* LossIcon;
	CSprite* TownPix;
	CSprite* Resource;
	CSprite* heroSpecificAbility;
	Bitmap816* GoldBox;
	Bitmap816* Flags[8];
	Bitmap816* Panels[8];
	Bitmap816* HeroPix[163];
	Bitmap816* randomTownQuestion;
	Bitmap816* randomHeroQuestion;
	Bitmap816* randomTown;
	Bitmap816* randomHero;
	Bitmap816* noDice;
	Bitmap816* noHero;
	int sortDirection;
	int currentIndex;
	int currentMap;
	int durationIndex;
	unsigned char inAdvancedOptions;
	unsigned char inScenarioOptions;
	unsigned char inRMGOptions;
	textEntryWidget* edit380;//H3DlgEdit* edit380;
	int _f_384;
	int _f_388;
	H3ScenarioMapInformation mapInfo;
	std::vector<H3ScenarioMapInformation> vector1030;
	std::vector<H3ScenarioMapInformation> vector1040;
	std::vector<H3ScenarioMapInformation> mapsInformation;
	H3ScenarioMapInformation* mapsInfoPtr;
	H3ScenarioPlayer mapPlayersHuman[8];
	H3ScenarioPlayer mapPlayersComputer[8];
	textEntryWidget* saveGameEdit;
	unsigned char mode;
	CNewPlayerUpdateMan* pNewPlayerUpdateMan;
	CNetPlayerHandler netPlayerHandler;
	unsigned char receivedMaps;
	slider* chatSlider;//H3DlgScrollbar* scrollBar1838;
	slider* fileSlider;//H3DlgScrollbar* scrollBar183C;
	slider* durationSlider;//H3DlgScrollbar* turnDurationScroll;
	slider* nameSlider;
	textWidget* chatWidget;//H3DlgText* text1848;
	textWidget* nameList1;//H3DlgText* text184C;
	textWidget* nameList2;//H3DlgText* text1850;
	bool mapChanged;
	CChatEdit* chatEdit;//H3DlgEdit* edit1858;
	int sortWhich;
	int filterSize;
	unsigned char scenarioOptionsStarted;
	unsigned char chatShowing;
	textButton* chatToggle;//H3DlgDefButton* button1868;
	unsigned char receivingMaps;
	CSaveScreen* flagBack;//H3LoadedPcx16* extendedPcx;
	char gameVersion[20];
	CSingleSelectionNetMsgHandler netMsgHandler;//h3func* newGameCampaignVtable;
	char _f_188C[8];
	char _f_1894;
	int iGameVersion;
	int _f_189C;
	unsigned int mapDimension;
	int numberLevels;
	int numberPlayersSelected;
	int _f_18AC;
	int computerPlayersOnlySelected;
	int _f_18B4;
	int waterContentSelected;
	int monsterStrengthSelected;
	textButton* humanComputerButtons[9];//H3DlgDefButton* humanComputerButtons[9];
	textButton* humanComputerTeamsButtons[9];//H3DlgDefButton* humanComputerTeamsButtons[9];
	textButton* computerOnlyButtons[9];//H3DlgDefButton* computerOnlyButtons[9];
	textButton* computerOnlyTeams[8];//H3DlgDefButton* computerOnlyTeams[8];
	textButton* waterContentButtons[4];//H3DlgDefButton* waterContentButtons[4];
	textButton* monsterStrengthButtons[4];//H3DlgDefButton* monsterStrengthButtons[4];
	slider* textScroll;//H3DlgScrollableText* textScroll;

	inline char* const TSingleSelectionWindow::GetHeroName(int gamePos)
	{
		return CALL_2(char* const, __thiscall, 0x58D4C0, this, gamePos);
	}

	inline int TSingleSelectionWindow::CalcPosition(int playerPos)
	{
		int playerNum = 0;

		for (int i = 0; i < playerPos; ++i)
		{
		  if (gpGame->sSetup.playerPos[i] >= 0 && (!this->isCampaign || !gpGame->playerDead[i]))
			++playerNum;
		}

		return playerNum;
	}
};
#pragma pack(pop)
// ===============================================================

// Debug
inline void debugStr(const char* format, ...)
{
   va_list args;
   va_start(args, format);

   vsprintf(o_TextBuffer, format, args);
   b_MsgBox(o_TextBuffer, MBX_OK);
   
   va_end(args);
}

inline void debugStrWin(const char* format, ...)
{
   va_list args;
   va_start(args, format);

   vsprintf(o_TextBuffer, format, args);
   ShowMessage(o_TextBuffer);
   
   va_end(args);
}
