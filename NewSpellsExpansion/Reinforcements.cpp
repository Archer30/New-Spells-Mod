#include "stdafx.h"
#include "../NewSpells/NewSpells.h"
#include "../NewSpells/NewSpellsProviderApi.h"
#include "Reinforcements.h"

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static_assert(sizeof(armyGroup) == 0x38,
   "Hero army-group ABI mismatch");
static_assert(sizeof(_Army_) == 0x38,
   "Native army ABI mismatch");
static_assert(offsetof(hero, heroArmy) == 0x91,
   "Hero army offset mismatch");
static_assert(offsetof(_Town_, guards) == 0xE0,
   "Town guard-army offset mismatch");
static_assert(sizeof(_Town_) == 0x168,
   "Town structure stride mismatch");
static_assert(sizeof(stdString) == 0x10,
   "Heroes III string ABI mismatch");
static_assert(sizeof(_TownMgr_) == 0x1D8,
   "Town-manager ABI mismatch");
static_assert(offsetof(_TownMgr_, town) == 0x38,
   "Town-manager current-town offset mismatch");
static_assert(offsetof(_TownMgr_, garribar_up) == 0x11C,
   "Town-manager upper army-bar offset mismatch");
static_assert(offsetof(_TownMgr_, garribar_down) == 0x120,
   "Town-manager lower army-bar offset mismatch");
static_assert(offsetof(_TownMgr_, garribar_src) == 0x12C,
   "Town-manager source army-bar offset mismatch");
static_assert(offsetof(_TownMgr_, garribar_dst) == 0x134,
   "Town-manager destination army-bar offset mismatch");
static_assert(offsetof(_GarrisonBar_, army) == 0x6C,
   "Garrison-bar army offset mismatch");

namespace
{
   const int REINFORCEMENT_SLOT_COUNT = 7;
   const int REINFORCEMENT_HERO_COUNT = 156;
   const int REINFORCEMENT_TOWNS_PER_PAGE = 12;
   const int REINFORCEMENT_NO_CREATURE = -1;
   const int REINFORCEMENT_GARRISON_WINDOW_SIZE = 0x78;
   const int REINFORCEMENT_TOWN_CANCELLED = -1;
   const int REINFORCEMENT_TOWN_DENIED = -2;

   const int ARMY_COMMAND_MERGE = 2;
   const int ARMY_COMMAND_EXCHANGE = 3;
   const int ARMY_COMMAND_SPLIT = 5;
   const int GARRISON_TITLE_WIDGET_ID = 203;

   const int DLG_CANCEL = 30721;
   const int DLG_PREVIOUS = 30723;
   const int DLG_NEXT = 30724;
   const int DLG_TOWN_FIRST = 1000;

   char GARRISON_PCX[] = "garrison.pcx";
   char TOWN_PORTRAIT_DEF[] = "itpt.def";
   char CANCEL_BUTTON_DEF[] = "iCancel.def";
   char TEXT_BUTTON_DEF[] = "GSPBUT2.DEF";
   char BIG_FONT[] = "bigfont.fnt";
   char SMALL_FONT[] = "smalfont.fnt";

   const char REINFORCEMENT_SAVE_SECTION[] = "NewSpells.Reinforcements";
   const std::uint32_t REINFORCEMENT_SAVE_MAGIC = 0x31494552; // REI1
   const std::uint32_t REINFORCEMENT_SAVE_VERSION = 1;

   unsigned char reinforcementCastCount[REINFORCEMENT_HERO_COUNT];

   struct ReinforcementSaveData
   {
      std::uint32_t magic;
      std::uint32_t version;
      unsigned char castCount[REINFORCEMENT_HERO_COUNT];
   };

   static_assert(sizeof(ReinforcementSaveData) == 164,
      "Reinforcements save section ABI mismatch");

   enum StackOrigin
   {
      STACK_EMPTY,
      STACK_HERO_ORIGIN,
      STACK_TOWN_RETURNABLE,
      STACK_MIXED
   };

   enum ArmyBarRole
   {
      BAR_UNKNOWN,
      BAR_TOWN,
      BAR_HERO
   };

   struct NativeTransferState
   {
      void* window;
      hero* castingHero;
      _Town_* town;
      _Army_* sourceArmy;
      _Army_* destinationArmy;
      std::string header;
      StackOrigin bottomOrigin[REINFORCEMENT_SLOT_COUNT];
   };

   struct ArmySlotSnapshot
   {
      int type;
      int count;
   };

   struct NativeGarrisonWindowStorage
   {
      std::uint32_t words[REINFORCEMENT_GARRISON_WINDOW_SIZE /
         sizeof(std::uint32_t)];
   };

   static_assert(sizeof(NativeGarrisonWindowStorage) ==
      REINFORCEMENT_GARRISON_WINDOW_SIZE,
      "Native garrison-window storage mismatch");

   NativeTransferState* activeNativeTransfer = 0;
   HiHook* reinforcementCommandHook = 0;
   bool reinforcementLifecycleHandlersRegistered = false;

   template <std::size_t Size>
   bool matchesReinforcementCode(const std::uintptr_t address,
      const unsigned char (&signature)[Size])
   {
      return std::memcmp(reinterpret_cast<const void*>(address), signature,
         Size) == 0;
   }

   template <std::size_t Size>
   bool hookChainCompatible(const std::uintptr_t address,
      const unsigned char (&originalEntry)[Size])
   {
      Patch* existing = _P ? _P->GetFirstPatchAt(address) : 0;
      if (!existing)
         return matchesReinforcementCode(address, originalEntry);

      for (; existing; existing = existing->GetAppliedAfter())
      {
         if (!existing->IsApplied() || existing->GetType() != HIHOOK_ ||
             existing->GetAddress() != address || existing->GetSize() < 6)
            return false;
      }

      return true;
   }

   bool commandHookChainCompatible()
   {
      const unsigned char originalEntry[] =
         {0x55,0x8B,0xEC,0x8B,0x45,0x08};
      return hookChainCompatible(0x5D5010, originalEntry);
   }

   bool nativeGarrisonProfileValid()
   {
      const unsigned char constructor[] =
         {0x55,0x8B,0xEC,0x6A,0xFF,0x68,0xAE,0x62,0x63,0x00,
          0x64,0xA1,0x00,0x00,0x00,0x00};
      const unsigned char destructor[] =
         {0x55,0x8B,0xEC,0x6A,0xFF,0x68,0xC8,0x62,0x63,0x00,
          0x64,0xA1,0x00,0x00,0x00,0x00};
      const unsigned char commandBody[] =
         {0x53,0x56,0x83,0xF8,0x09,0x57,0x8B,0xF1,0x0F,0x87};
      const unsigned char resetSelection[] =
         {0x56,0x8B,0xF1,0x57,0xBF,0xFE,0xFF,0xFF,0xFF,
          0x8B,0x86,0x2C,0x01,0x00,0x00};
      const unsigned char getWidget[] =
         {0x55,0x8B,0xEC,0x8B,0x41,0x2C,0x85,0xC0,0x74,0x12};
      const unsigned char setText[] =
         {0x55,0x8B,0xEC,0x53,0x56,0x8B,0x75,0x08,0x57,
          0x8D,0x59,0x30};
      const unsigned char doModalBody[] =
         {0x25,0xFF,0x00,0x00,0x00,0x50,0x68,0xC0,0xFA,0x5F,0x00};

      return matchesReinforcementCode(0x5D1200, constructor) &&
         matchesReinforcementCode(0x5D14C0, destructor) &&
         matchesReinforcementCode(0x5D5016, commandBody) &&
         matchesReinforcementCode(0x5D5930, resetSelection) &&
         matchesReinforcementCode(0x5FF5B0, getWidget) &&
         matchesReinforcementCode(0x57CA50, setText) &&
         matchesReinforcementCode(0x5FFA26, doModalBody) &&
         commandHookChainCompatible();
   }

   void addItem(_Dlg_* dlg, _DlgItem_* item)
   {
      if (dlg && item)
         dlg->AddItem(item);
   }

   std::string translated(const char* key)
   {
      const char* const value = Era::tr(key);
      return value && std::strcmp(value, key) != 0 ? value : std::string();
   }

   std::string reinforcementText(const char* suffix)
   {
      const std::string currentKey =
         std::string("NewSpellsExpansion.Spells.Reinforcements.") + suffix;
      std::string value = translated(currentKey.c_str());
      if (!value.empty())
         return value;

      const std::string legacyKey =
         std::string("NewSpells.Reinforcements.") + suffix;
      return translated(legacyKey.c_str());
   }

   std::string reinforcementText(const char* suffix,
      const std::vector<std::string>& params)
   {
      const std::string currentKey =
         std::string("NewSpellsExpansion.Spells.Reinforcements.") + suffix;
      std::string value = Era::tr(currentKey.c_str(), params);
      if (!value.empty() && value != currentKey)
         return value;

      const std::string legacyKey =
         std::string("NewSpells.Reinforcements.") + suffix;
      value = Era::tr(legacyKey.c_str(), params);
      return !value.empty() && value != legacyKey ? value : std::string();
   }

   void showReinforcementMessage(const char* suffix)
   {
      std::string message = reinforcementText(suffix);
      if (!message.empty())
         b_MsgBox(const_cast<char*>(message.c_str()), MBX_OK);
   }

   bool validHeroId(const int heroId)
   {
      return heroId >= 0 && heroId < REINFORCEMENT_HERO_COUNT;
   }

   hero* getHero(const int heroId)
   {
      return pGame && validHeroId(heroId)
         ? reinterpret_cast<hero*>(pGame->GetHero(heroId)) : 0;
   }

   _Army_* getHeroArmy(hero* Hero)
   {
      return Hero ? reinterpret_cast<_Army_*>(&Hero->heroArmy) : 0;
   }

   hero* getTownSourceHero(_Town_* town)
   {
      return town && validHeroId(town->up_hero_id)
         ? getHero(town->up_hero_id) : 0;
   }

   _Army_* getTownSourceArmy(_Town_* town)
   {
      if (!town)
         return 0;

      hero* const garrisonHero = getTownSourceHero(town);
      return garrisonHero ? getHeroArmy(garrisonHero) : &town->guards;
   }

   std::string getTownName(const _Town_* town)
   {
      if (!town)
         return std::string();

      // The legacy _Town_ declaration models bytes 0xC8..0xD3 as char[12].
      // In the executable, 0xC4 is a 16-byte MSVC std::string and 0xC8 is its
      // internal text pointer. Treating town->name as text produced the four
      // corrupt pointer glyphs shown in the reported dialog title.
      const stdString* const name = reinterpret_cast<const stdString*>(
         reinterpret_cast<const unsigned char*>(town) + 0xC4);
      if (!name->text || name->length > name->capacity ||
          name->length > 1024)
         return std::string();

      return std::string(name->text, name->length);
   }

   std::vector<int> getOwnedTownIds(const hero* castingHero)
   {
      std::vector<int> result;
      if (!castingHero || !pGame || castingHero->playerOwner < 0 ||
          castingHero->playerOwner >= 8)
         return result;

      _Player_* const player = pGame->GetPlayer(castingHero->playerOwner);
      if (!player)
         return result;

      const int gameTownCount = pGame->GetTownsCount();
      const int playerTownCount = static_cast<unsigned char>(player->towns_count);
      for (int i = 0; i < playerTownCount && i < 48; ++i)
      {
         const int townId = static_cast<unsigned char>(player->towns_ids[i]);
         if (townId < 0 || townId >= gameTownCount)
            continue;

         _Town_* const town = pGame->GetTown(townId);
         if (!town || town->owner_id != castingHero->playerOwner ||
             town->up_hero_id == castingHero->id)
            continue;

         result.push_back(townId);
      }

      return result;
   }

   int getNearestTownId(const hero* castingHero,
      const std::vector<int>& townIds)
   {
      int nearestTownId = -1;
      long nearestDistance = LONG_MAX;

      for (std::size_t i = 0; i < townIds.size(); ++i)
      {
         _Town_* const town = pGame->GetTown(townIds[i]);
         if (!town)
            continue;

         const long dx = static_cast<long>(town->x) - castingHero->mapX;
         const long dy = static_cast<long>(town->y) - castingHero->mapY;
         const long dz = static_cast<long>(town->z) - castingHero->mapZ;
         const long distance = dx * dx + dy * dy + dz * dz * 100000L;
         if (distance < nearestDistance)
         {
            nearestDistance = distance;
            nearestTownId = townIds[i];
         }
      }

      return nearestTownId;
   }

   int showTownSelectionPage(const std::vector<int>& townIds, const int page)
   {
      _Dlg_* const dlg = _CustomDlg_::Create(DLG_X_CENTER, DLG_Y_CENTER,
         549, 395, DF_SCREENSHOT | DF_SHADOW, 0);
      if (!dlg)
         return DLG_CANCEL;

      addItem(dlg, _DlgStaticPcx8_::Create(0, 0, 549, 395, 900,
         GARRISON_PCX));

      std::string title = reinforcementText("Dialog.Text");
      addItem(dlg, _DlgStaticText_::Create(20, 18, 509, 36,
         const_cast<char*>(title.c_str()), BIG_FONT, 2, 901,
         ALIGN_H_CENTER | ALIGN_V_CENTER, 0));

      std::string labels[REINFORCEMENT_TOWNS_PER_PAGE];
      const int firstTown = page * REINFORCEMENT_TOWNS_PER_PAGE;
      for (int i = 0; i < REINFORCEMENT_TOWNS_PER_PAGE; ++i)
      {
         const int townIndex = firstTown + i;
         if (townIndex >= static_cast<int>(townIds.size()))
            break;

         _Town_* const town = pGame->GetTown(townIds[townIndex]);
         if (!town)
            continue;

         const int column = i % 4;
         const int row = i / 4;
         const int x = 44 + column * 126;
         const int y = 65 + row * 88;
         int frame = static_cast<int>(town->type) * 2;
         if (frame < 0)
            frame = 0;

         addItem(dlg, _DlgButton_::Create(x + 20, y, 58, 64,
            DLG_TOWN_FIRST + i, TOWN_PORTRAIT_DEF, frame, frame, true, 0,
            DIF_BUTTON));

         labels[i] = getTownName(town);
         addItem(dlg, _DlgStaticText_::Create(x, y + 63, 98, 20,
            const_cast<char*>(labels[i].c_str()), SMALL_FONT, 1,
            910 + i, ALIGN_H_CENTER | ALIGN_V_CENTER, 0));
      }

      const int pageCount = static_cast<int>((townIds.size() +
         REINFORCEMENT_TOWNS_PER_PAGE - 1) /
         REINFORCEMENT_TOWNS_PER_PAGE);
      char pageBuffer[32];
      sprintf_s(pageBuffer, sizeof(pageBuffer), "%d / %d", page + 1,
         pageCount);
      addItem(dlg, _DlgStaticText_::Create(230, 337, 89, 24, pageBuffer,
         SMALL_FONT, 1, 950, ALIGN_H_CENTER | ALIGN_V_CENTER, 0));

      std::string previous = reinforcementText("Dialog.Previous");
      std::string next = reinforcementText("Dialog.Next");
      if (page > 0)
         addItem(dlg, _DlgTextButton_::Create(34, 337, DLG_PREVIOUS,
            TEXT_BUTTON_DEF, const_cast<char*>(previous.c_str()), SMALL_FONT,
            0, 1, true, 0, 1));
      if (page + 1 < pageCount)
         addItem(dlg, _DlgTextButton_::Create(414, 337, DLG_NEXT,
            TEXT_BUTTON_DEF, const_cast<char*>(next.c_str()), SMALL_FONT,
            0, 1, true, 0, 1));

      addItem(dlg, _DlgButton_::Create(243, 363, 64, 30, DLG_CANCEL,
         CANCEL_BUTTON_DEF, 0, 1, true, 1, DIF_BUTTON));

      o_WndMgr->result_dlg_item_id = -1;
      dlg->Run();
      const int result = o_WndMgr->result_dlg_item_id;
      dlg->Destroy(TRUE);
      return result;
   }

   int chooseTown(const hero* castingHero, const int schoolLevel)
   {
      const std::vector<int> townIds = getOwnedTownIds(castingHero);
      if (townIds.empty())
      {
         showReinforcementMessage("Dialog.Error.NoTown");
         return REINFORCEMENT_TOWN_DENIED;
      }

      if (schoolLevel < eMasteryAdvanced || townIds.size() == 1)
         return getNearestTownId(castingHero, townIds);

      int page = 0;
      const int pageCount = static_cast<int>((townIds.size() +
         REINFORCEMENT_TOWNS_PER_PAGE - 1) /
         REINFORCEMENT_TOWNS_PER_PAGE);

      while (page >= 0 && page < pageCount)
      {
         const int result = showTownSelectionPage(townIds, page);
         if (result >= DLG_TOWN_FIRST &&
             result < DLG_TOWN_FIRST + REINFORCEMENT_TOWNS_PER_PAGE)
         {
            const int index = page * REINFORCEMENT_TOWNS_PER_PAGE +
               result - DLG_TOWN_FIRST;
            return index >= 0 && index < static_cast<int>(townIds.size())
               ? townIds[index] : REINFORCEMENT_TOWN_CANCELLED;
         }
         if (result == DLG_PREVIOUS && page > 0)
         {
            --page;
            continue;
         }
         if (result == DLG_NEXT && page + 1 < pageCount)
         {
            ++page;
            continue;
         }
         break;
      }

      return REINFORCEMENT_TOWN_CANCELLED;
   }

   bool validSlot(const int slot)
   {
      return slot >= 0 && slot < REINFORCEMENT_SLOT_COUNT;
   }

   bool isOccupied(const _Army_* army, const int slot)
   {
      return army && validSlot(slot) &&
         army->type[slot] != REINFORCEMENT_NO_CREATURE &&
         army->count[slot] > 0;
   }

   ArmySlotSnapshot snapshotSlot(const _Army_* army, const int slot)
   {
      ArmySlotSnapshot snapshot = {REINFORCEMENT_NO_CREATURE, 0};
      if (army && validSlot(slot))
      {
         snapshot.type = army->type[slot];
         snapshot.count = army->count[slot];
      }
      return snapshot;
   }

   bool slotChanged(const _Army_* army, const int slot,
      const ArmySlotSnapshot& before)
   {
      return !army || !validSlot(slot) ||
         army->type[slot] != before.type || army->count[slot] != before.count;
   }

   StackOrigin combineOrigins(const StackOrigin left,
      const StackOrigin right)
   {
      if (left == STACK_EMPTY)
         return right;
      if (right == STACK_EMPTY)
         return left;
      if (left == right)
         return left;
      if (left == STACK_MIXED || right == STACK_MIXED)
         return STACK_MIXED;
      return STACK_MIXED;
   }

   ArmyBarRole getBarRole(const NativeTransferState& state,
      const _TownMgr_* manager, const _GarrisonBar_* bar)
   {
      if (!manager || !bar)
         return BAR_UNKNOWN;
      if (bar == manager->garribar_up && bar->army == state.sourceArmy)
         return BAR_TOWN;
      if (bar == manager->garribar_down &&
          bar->army == state.destinationArmy)
         return BAR_HERO;
      return BAR_UNKNOWN;
   }

   StackOrigin getOrigin(const NativeTransferState& state,
      const ArmyBarRole role, const _Army_* army, const int slot)
   {
      if (!isOccupied(army, slot))
         return STACK_EMPTY;
      return role == BAR_HERO ? state.bottomOrigin[slot] :
         (role == BAR_TOWN ? STACK_TOWN_RETURNABLE : STACK_EMPTY);
   }

   bool provenanceConsistent(const NativeTransferState& state)
   {
      for (int slot = 0; slot < REINFORCEMENT_SLOT_COUNT; ++slot)
      {
         const bool occupied = isOccupied(state.destinationArmy, slot);
         if (occupied != (state.bottomOrigin[slot] != STACK_EMPTY))
            return false;
      }
      return true;
   }

   void setBottomOrigin(NativeTransferState& state, const int slot,
      const StackOrigin origin)
   {
      if (validSlot(slot))
         state.bottomOrigin[slot] = isOccupied(state.destinationArmy, slot)
            ? origin : STACK_EMPTY;
   }

   bool isMutationCommand(const int command)
   {
      return command == ARMY_COMMAND_MERGE ||
         command == ARMY_COMMAND_EXCHANGE ||
         command == ARMY_COMMAND_SPLIT;
   }

   void resetNativeSelection(_TownMgr_* manager)
   {
      if (manager)
         CALL_1(void, __thiscall, 0x5D5930, manager);
   }

   void callDefaultArmyCommand(HiHook* hook, _TownMgr_* manager,
      const int command, const int isGarrison, void* window)
   {
      CALL_4(void, __thiscall, hook->GetDefaultFunc(), manager, command,
         isGarrison, window);
   }

   void reportActiveProfileMismatch(_TownMgr_* manager)
   {
      resetNativeSelection(manager);
      reinforcementNativeReady = false;
   }

   bool commandPermitted(const NativeTransferState& state,
      const int command, const ArmyBarRole sourceRole,
      const ArmyBarRole destinationRole, const int sourceSlot,
      const int destinationSlot)
   {
      if (sourceRole == BAR_HERO && destinationRole == BAR_TOWN &&
          state.bottomOrigin[sourceSlot] != STACK_TOWN_RETURNABLE)
         return false;

      // Exchanging a town stack with an original/mixed hero stack would move
      // that hero-owned stack into the town. Empty slots and stacks already
      // borrowed from the town are safe exchange destinations.
      if (command == ARMY_COMMAND_EXCHANGE &&
          sourceRole == BAR_TOWN && destinationRole == BAR_HERO)
      {
         const StackOrigin destinationOrigin =
            state.bottomOrigin[destinationSlot];
         if (destinationOrigin != STACK_EMPTY &&
             destinationOrigin != STACK_TOWN_RETURNABLE)
            return false;
      }

      return true;
   }

   void updateOriginsAfterCommand(NativeTransferState& state,
      const int command, const ArmyBarRole sourceRole,
      const ArmyBarRole destinationRole, const int sourceSlot,
      const int destinationSlot, const StackOrigin sourceOrigin,
      const StackOrigin destinationOrigin,
      const ArmySlotSnapshot& sourceBefore,
      const ArmySlotSnapshot& destinationBefore)
   {
      _Army_* const sourceArmy = sourceRole == BAR_HERO
         ? state.destinationArmy : state.sourceArmy;
      _Army_* const destinationArmy = destinationRole == BAR_HERO
         ? state.destinationArmy : state.sourceArmy;

      const bool sourceChanged = slotChanged(sourceArmy, sourceSlot,
         sourceBefore);
      const bool destinationChanged = slotChanged(destinationArmy,
         destinationSlot, destinationBefore);
      if (!sourceChanged && !destinationChanged)
         return;

      if (command == ARMY_COMMAND_MERGE)
      {
         if (sourceRole == BAR_HERO)
            setBottomOrigin(state, sourceSlot, sourceOrigin);

         if (destinationRole == BAR_HERO && destinationChanged)
         {
            const StackOrigin merged = destinationBefore.type !=
               REINFORCEMENT_NO_CREATURE && destinationBefore.count > 0
               ? combineOrigins(destinationOrigin, sourceOrigin)
               : sourceOrigin;
            setBottomOrigin(state, destinationSlot, merged);
         }
         return;
      }

      if (command == ARMY_COMMAND_EXCHANGE)
      {
         if (sourceRole == BAR_HERO && destinationRole == BAR_HERO)
         {
            setBottomOrigin(state, sourceSlot, destinationOrigin);
            setBottomOrigin(state, destinationSlot, sourceOrigin);
         }
         else if (sourceRole == BAR_HERO)
            setBottomOrigin(state, sourceSlot, destinationOrigin);
         else if (destinationRole == BAR_HERO)
            setBottomOrigin(state, destinationSlot, sourceOrigin);
         return;
      }

      if (command == ARMY_COMMAND_SPLIT)
      {
         if (sourceRole == BAR_HERO)
            setBottomOrigin(state, sourceSlot, sourceOrigin);

         if (destinationRole == BAR_HERO && destinationChanged)
         {
            const StackOrigin splitOrigin = destinationBefore.type !=
               REINFORCEMENT_NO_CREATURE && destinationBefore.count > 0
               ? combineOrigins(destinationOrigin, sourceOrigin)
               : sourceOrigin;
            setBottomOrigin(state, destinationSlot, splitOrigin);
         }
      }
   }

   void __stdcall reinforcementArmyCommand(HiHook* hook,
      _TownMgr_* manager, const int command, const int isGarrison,
      void* window)
   {
      NativeTransferState* const state = activeNativeTransfer;
      if (!state || state->window != window || manager != o_TownMgr ||
          !isMutationCommand(command))
      {
         callDefaultArmyCommand(hook, manager, command, isGarrison, window);
         return;
      }

      if (!reinforcementNativeReady)
      {
         resetNativeSelection(manager);
         return;
      }

      _GarrisonBar_* const sourceBar = manager->garribar_src;
      _GarrisonBar_* const destinationBar = manager->garribar_dst;
      const int sourceSlot = manager->garribar_slot_index_src;
      const int destinationSlot = manager->garribar_slot_index_dst;
      const ArmyBarRole sourceRole = getBarRole(*state, manager, sourceBar);
      const ArmyBarRole destinationRole = getBarRole(*state, manager,
         destinationBar);

      if (!validSlot(sourceSlot) || !validSlot(destinationSlot) ||
          sourceRole == BAR_UNKNOWN || destinationRole == BAR_UNKNOWN ||
          !provenanceConsistent(*state))
      {
         reportActiveProfileMismatch(manager);
         return;
      }

      _Army_* const sourceArmy = sourceBar->army;
      _Army_* const destinationArmy = destinationBar->army;
      const StackOrigin sourceOrigin = getOrigin(*state, sourceRole,
         sourceArmy, sourceSlot);
      const StackOrigin destinationOrigin = getOrigin(*state,
         destinationRole, destinationArmy, destinationSlot);

      if (sourceOrigin == STACK_EMPTY)
      {
         reportActiveProfileMismatch(manager);
         return;
      }

      if (!commandPermitted(*state, command, sourceRole, destinationRole,
          sourceSlot, destinationSlot))
      {
         resetNativeSelection(manager);
         showReinforcementMessage("Dialog.Error.WrongTarget");
         return;
      }

      const ArmySlotSnapshot sourceBefore = snapshotSlot(sourceArmy,
         sourceSlot);
      const ArmySlotSnapshot destinationBefore = snapshotSlot(
         destinationArmy, destinationSlot);

      callDefaultArmyCommand(hook, manager, command, isGarrison, window);

      if (activeNativeTransfer == state)
         updateOriginsAfterCommand(*state, command, sourceRole,
            destinationRole, sourceSlot, destinationSlot, sourceOrigin,
            destinationOrigin, sourceBefore, destinationBefore);
   }

   void initializeBottomOrigins(NativeTransferState& state)
   {
      for (int slot = 0; slot < REINFORCEMENT_SLOT_COUNT; ++slot)
         state.bottomOrigin[slot] = isOccupied(state.destinationArmy, slot)
            ? STACK_HERO_ORIGIN : STACK_EMPTY;
   }

   bool containsBorrowedTownTroops(const NativeTransferState& state)
   {
      for (int slot = 0; slot < REINFORCEMENT_SLOT_COUNT; ++slot)
      {
         if (isOccupied(state.destinationArmy, slot) &&
             (state.bottomOrigin[slot] == STACK_TOWN_RETURNABLE ||
              state.bottomOrigin[slot] == STACK_MIXED))
            return true;
      }
      return false;
   }

   void destroyNativeGarrisonWindow(void* window)
   {
      if (window)
         CALL_1(void, __thiscall, 0x5D14C0, window);
   }

   void cleanupNativeTransferDialog(NativeTransferState* const state,
      _TownMgr_* const manager, _Town_* const previousTown)
   {
      activeNativeTransfer = 0;

      __try
      {
         if (state)
            destroyNativeGarrisonWindow(state->window);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         reinforcementNativeReady = false;
      }

      __try
      {
         if (manager)
            manager->town = previousTown;
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         reinforcementNativeReady = false;
      }
   }

   bool runNativeTransferDialog(NativeTransferState* const state,
      _TownMgr_* const manager, _Town_* const previousTown)
   {
      bool transferred = false;
      __try
      {
         manager->town = state->town;
         widget* const titleWidget = CALL_2(widget*, __thiscall, 0x5FF5B0,
            state->window, GARRISON_TITLE_WIDGET_ID);
         if (titleWidget && !state->header.empty())
         {
            CALL_2(void, __thiscall, 0x57CA50, titleWidget,
               state->header.c_str());
            activeNativeTransfer = state;
            CALL_2(void, __thiscall, 0x5FFA20, state->window, false);
            activeNativeTransfer = 0;
            transferred = containsBorrowedTownTroops(*state);
         }
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         // Keep a persisted transfer transactional even when a native dialog
         // call faults; otherwise report unsupported and disable this route.
         reinforcementNativeReady = false;
         __try
         {
            transferred = containsBorrowedTownTroops(*state);
         }
         __except (EXCEPTION_EXECUTE_HANDLER)
         {
            transferred = false;
         }
      }

      cleanupNativeTransferDialog(state, manager, previousTown);
      return transferred;
   }

   bool showTransferDialog(hero* castingHero, _Town_* town)
   {
      NativeTransferState state = {};
      state.castingHero = castingHero;
      state.town = town;
      state.sourceArmy = getTownSourceArmy(town);
      state.destinationArmy = getHeroArmy(castingHero);

      _TownMgr_* const manager = o_TownMgr;
      if (!state.sourceArmy || !state.destinationArmy || !manager)
         return false;

      initializeBottomOrigins(state);
      const std::string townName = getTownName(town);
      if (townName.empty())
         return false;

      NativeGarrisonWindowStorage storage = {};
      _Town_* const previousTown = manager->town;
      state.window = CALL_4(void*, __thiscall, 0x5D1200, &storage,
         castingHero, castingHero->playerOwner, state.sourceArmy);
      if (!state.window)
         return false;

      state.header = reinforcementText("Dialog.Header",
         std::vector<std::string>{"townName", townName});
      if (state.header.empty())
      {
         cleanupNativeTransferDialog(&state, manager, previousTown);
         return false;
      }

      // A compatibility check may fail after a transfer command has already
      // mutated the armies.  In that case the persisted transfer must still
      // be committed so the shared caster epilogue charges mana and this
      // provider charges movement/daily use exactly once.
      return runNativeTransferDialog(&state, manager, previousTown);
   }

   bool installReinforcementCommandHook()
   {
      if (reinforcementCommandHook)
         return reinforcementCommandHook->IsApplied() != 0;
      if (!_PI)
         return false;

      reinforcementCommandHook = _PI->CreateHiHook(0x5D5010, SPLICE_,
         EXTENDED_, THISCALL_, reinforcementArmyCommand);
      if (!reinforcementCommandHook)
         return false;

      reinforcementCommandHook->Apply();
      if (reinforcementCommandHook->IsApplied())
         return true;

      reinforcementCommandHook->Destroy();
      reinforcementCommandHook = 0;
      return false;
   }

   void migrateLegacyReinforcementCounts()
   {
      char key[96];
      for (int heroId = 0; heroId < REINFORCEMENT_HERO_COUNT; ++heroId)
      {
         sprintf_s(key, sizeof(key), "rei_summon_casted_by_hero_%d", heroId);
         const int count = Era::GetAssocVarIntValue(key);
         reinforcementCastCount[heroId] = static_cast<unsigned char>(
            count < 0 ? 0 : (count > 255 ? 255 : count));
      }
   }

   void refreshAdventureMapAfterCommit()
   {
      __try
      {
         if (pAdventureManager)
            pAdventureManager->FullUpdate(true);
      }
      __except (EXCEPTION_EXECUTE_HANDLER)
      {
         // Troops, movement, and daily use are already committed.  A failed
         // nonessential redraw must not turn the transaction into a free cast.
         reinforcementNativeReady = false;
      }
   }

   void __stdcall resetReinforcementEveryDay(Era::TEvent* event)
   {
      resetNativeReinforcementState();
   }

   void __stdcall resetReinforcementOnGameLeave(Era::TEvent* event)
   {
      resetNativeReinforcementState();
   }
}

bool reinforcementNativeReady = false;

_ptr_ _CustomDlg_::v_table_funcs[15] =
{
   0x41B040, 0x5FF0A0, 0x5FF220, 0x405610, 0x49A230,
   0x5FF5E0, 0x5FFA20, 0x5FFB30, 0x5FFBB0,
   reinterpret_cast<_ptr_>(_CustomDlg_::DlgProcBridge),
   0x5FFCA0, 0x5FFD50, 0x5FFE90, 0x4842C0, 0x41B0F0
};

_Dlg_* _CustomDlg_::Create(int x, int y, int width, int height,
   _dword_ flags, _func_CustomDlgProc dlgProc)
{
   _CustomDlg_* const dlg = reinterpret_cast<_CustomDlg_*>(
      o_New(sizeof(_CustomDlg_)));
   if (!dlg)
      return 0;

   if (x == DLG_X_CENTER)
      x = (o_WndMgr->screen_pcx16->width - width) / 2;
   if (y == DLG_Y_CENTER)
      y = (o_WndMgr->screen_pcx16->height - height) / 2;

   CALL_6(_Dlg_*, __thiscall, 0x41AFA0, dlg, x, y, width, height, flags);
   dlg->dlg_proc = dlgProc;
   dlg->v_table = v_table_funcs;
   return dlg;
}

int __fastcall _CustomDlg_::DlgProcBridge(_CustomDlg_* this_,
   _dword_ notUsed, _EventMsg_* msg)
{
   return this_->dlg_proc ? this_->dlg_proc(this_, msg) :
      this_->DefProc(msg);
}

bool initializeNativeReinforcements()
{
   if (!nativeGarrisonProfileValid())
      return false;
   if (!installReinforcementCommandHook())
      return false;

   resetNativeReinforcementState();
   return true;
}

bool installNativeReinforcementLifecycleHandlers()
{
   if (!Era::RegisterHandler)
      return false;
   if (reinforcementLifecycleHandlersRegistered)
      return true;

   // Named ERA lifecycle events avoid competing with other mods at the game
   // new-day/new-game function entries. OnEveryDay resets the daily quota;
   // leaving a map clears process-local state before either a fresh game or a
   // load, whose OnSavegameRead handler then restores the persisted counters.
   Era::RegisterHandler(resetReinforcementEveryDay, "OnEveryDay");
   Era::RegisterHandler(resetReinforcementOnGameLeave, "OnGameLeave");
   reinforcementLifecycleHandlersRegistered = true;
   return true;
}

void shutdownNativeReinforcements()
{
   // Setup is single-threaded during process attach/OnAfterWoG. Registration
   // failure rolls back the only executable hook. Named lifecycle handlers
   // cannot be unregistered, but merely clear this provider's private array.
   activeNativeTransfer = 0;
   if (reinforcementCommandHook)
   {
      reinforcementCommandHook->Destroy();
      reinforcementCommandHook = 0;
   }

   reinforcementNativeReady = false;
}

int castNativeReinforcements(hero* Hero, const int schoolLevel)
{
   if (!reinforcementNativeReady)
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   if (!Hero || !validHeroId(Hero->id) || schoolLevel < eMasteryNone ||
       schoolLevel > eMasteryExpert)
      return NEWSPELLS_PROVIDER_DENIED;

   const int minimumMovement = schoolLevel == eMasteryExpert ? 200 : 300;
   if (Hero->currMobility < minimumMovement)
   {
      showReinforcementMessage("MovementMessage");
      return NEWSPELLS_PROVIDER_DENIED;
   }

   const int dailyLimit = schoolLevel + 1;
   if (reinforcementCastCount[Hero->id] >= dailyLimit)
   {
      std::string message = reinforcementText("LimitMessage",
         std::vector<std::string>{"heroName", Hero->name, "limit",
            Era::IntToStr(dailyLimit)});
      if (!message.empty())
         b_MsgBox(const_cast<char*>(message.c_str()), MBX_OK);
      return NEWSPELLS_PROVIDER_DENIED;
   }

   const int townId = chooseTown(Hero, schoolLevel);
   if (townId == REINFORCEMENT_TOWN_DENIED || !pGame)
      return NEWSPELLS_PROVIDER_DENIED;
   if (townId == REINFORCEMENT_TOWN_CANCELLED)
      return NEWSPELLS_PROVIDER_CANCELLED;

   _Town_* const town = pGame->GetTown(townId);
   if (!town)
      return NEWSPELLS_PROVIDER_DENIED;
   if (!showTransferDialog(Hero, town))
      return reinforcementNativeReady ? NEWSPELLS_PROVIDER_CANCELLED :
         NEWSPELLS_PROVIDER_UNSUPPORTED;

   const int movementCost = ((5 - schoolLevel) * 2 / 3) * 100;
   Hero->currMobility = Hero->currMobility > movementCost
      ? Hero->currMobility - movementCost : 0;
   ++reinforcementCastCount[Hero->id];

   refreshAdventureMapAfterCommit();
   return NEWSPELLS_PROVIDER_COMMITTED;
}

void resetNativeReinforcementState()
{
   std::memset(reinforcementCastCount, 0, sizeof(reinforcementCastCount));
}

void __stdcall saveNativeReinforcementState(Era::TEvent* Event)
{
   ReinforcementSaveData data;
   data.magic = REINFORCEMENT_SAVE_MAGIC;
   data.version = REINFORCEMENT_SAVE_VERSION;
   std::memcpy(data.castCount, reinforcementCastCount,
      sizeof(data.castCount));
   Era::WriteSavegameSection(sizeof(data), &data,
      REINFORCEMENT_SAVE_SECTION);
}

void __stdcall loadNativeReinforcementState(Era::TEvent* Event)
{
   ReinforcementSaveData data;
   std::memset(&data, 0, sizeof(data));
   const int bytesRead = Era::ReadSavegameSection(sizeof(data), &data,
      REINFORCEMENT_SAVE_SECTION);

   if (bytesRead == 0)
   {
      migrateLegacyReinforcementCounts();
      return;
   }

   if (bytesRead != static_cast<int>(sizeof(data)) ||
       data.magic != REINFORCEMENT_SAVE_MAGIC ||
       data.version != REINFORCEMENT_SAVE_VERSION)
   {
      resetNativeReinforcementState();
      return;
   }

   std::memcpy(reinforcementCastCount, data.castCount,
      sizeof(reinforcementCastCount));
}
