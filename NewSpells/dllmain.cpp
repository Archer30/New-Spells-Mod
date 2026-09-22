// ===============================================================
// ---------------------- NEW SPELLS PLUGIN ----------------------
// ---------------- https://handbookhmm.ru/forum/ ----------------
// ===============================================================

#include "NewSpells.h"
#include "NewSpellsAiInterop.h"
#include "NewSpellsProviderApi.h"
#include "NewSpellsMapFormat.h"
#include "ErmBattleFields.h"

Patcher* _P;
PatcherInstance* _PI;

//std::string jsonKeyName;
//char jsonKeyName[MAX_PATH];
const std::size_t NEW_SPELL_COUNT = sizeof(newSpell) / sizeof(newSpell[0]);
#include "AnimationCatalog.h"
_Spell_ newSpellDefaults[SPELLS_MAX];
bool managedSpellDefaults[SPELLS_MAX] = {};
bool newSpellDefaultsReady = false;
int activeSpellCount = DEFAULT_SPELLS_NUM;
#define SPELLS_NUM activeSpellCount
_MagicAnim_ SpellAnim[ANIMS_MAX];
int customAnimationBase = ANIM_FEAR;
int activeAnimationCount = ANIMS_NUM;
NewSpellsMap::State mapDisabledSpells = {};
bool hasValidArmyCoordinates(const army* Army);
bool isRealArmy(const army* Army);
inline int& nsDuration(army* Army, int spell);
void nsCancelDurationsEx(army* Army, bool onlyNegative);
void nsNewRoundDurationsEx(army* Army);
bool nsHasActiveDurationEx(army* Army, bool helpfulOnly);
void nsApplyDataKindTables();
void nsDataClearMods();
army* findBattleStackAtHex(const int hex);
extern int activeSpellMastery[2][21][SPELLS_MAX];
int __stdcall creatureCast(LoHook* h, HookContext* c);
int __stdcall skipMeleeAttackUnderFear(LoHook* h, HookContext* c);
#ifdef NEWSPELLS_CEILING_NATIVE_PROBE
void RunCeilingNativeProbe();
void ceilingProbeTranslations(const char* stage);
void __stdcall ceilingProbeAfterPlugins(Era::TEvent* e);
void __stdcall ceilingProbeAfterWog(Era::TEvent* e);
void __stdcall ceilingProbeBeforeErm(Era::TEvent* e);
#endif
#ifdef NEWSPELLS_BMG_NATIVE_PROBE
void RunBmgNativeProbe();
#ifdef NEWSPELLS_BMG_BASELINE_PROBE
int __stdcall RunBmgBaselineProbe(LoHook*, HookContext*)
{
   RunBmgNativeProbe();
   return EXEC_DEFAULT;
}
#endif
#endif

const char MAP_DISABLED_SAVE_SECTION[] = "NewSpells.MapDisabled";
char AI_SPELL_QUERY_VARIABLE_NAME[] = NEWSPELLS_AI_SPELL_QUERY_VARIABLE_V1;
char SPELL_PROVIDER_REGISTRY_VARIABLE_NAME[] =
   NEWSPELLS_PROVIDER_REGISTRY_VARIABLE_V1;
char FEAR_MELEE_GUARD_OWNER_NAME[] =
   "HD.Plugin.H3.NewSpells.FearMeleeGuard.v1";

struct ExternalSpellSlot
{
   bool registered;
   bool active;
   bool faulted;
   bool contested;
   char providerKey[64];
   char spellKey[64];
   bool hasCustomAnimation;
   unsigned char combatTargetDispatch;
   char animationDef[64];
   char animationName[192];
   char animationKey[192];
   NewSpellsProviderSpellV1 descriptor;
};

enum ExternalCombatTargetMode
{
   EXTERNAL_TARGET_MODE_TARGETED,
   EXTERNAL_TARGET_MODE_AREA,
   EXTERNAL_TARGET_MODE_GLOBAL,
   EXTERNAL_TARGET_MODE_SUMMON
};

ExternalSpellSlot externalSpellSlots[
   NEWSPELLS_EXTERNAL_SPELL_LAST_ID - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID + 1] = {};
int externalAnimationIndex[
   NEWSPELLS_EXTERNAL_SPELL_LAST_ID - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID + 1];
bool providerRegistrationOpen = true;
bool combatProviderDispatcherInstalled = false;
bool combatProviderEpilogueInstalled = false;
bool combatProviderPrecheckInstalled = false;
bool combatProviderTargetRoutingInstalled = false;
bool combatProviderWorkChanceInstalled = false;
bool combatAiProviderHooksInstalled = false;
bool adventureCastProviderHookInstalled = false;
bool dispelProviderHooksInstalled = false;
bool cureProviderHookInstalled = false;
bool statusApplyProviderHooksInstalled = false;
bool statusRoundProviderHookInstalled = false;
bool statusRemoveProviderHookInstalled = false;
bool battleLifecycleProviderHooksInstalled = false;
bool adventureAiProviderHookInstalled = false;
std::uint32_t providerRuntimeCapabilities = NEWSPELLS_CAP_ALL_V1;

struct PendingExternalCombatTransaction
{
   bool valid;
   std::uintptr_t frame;
   int spellId;
   int casterKind;
   int casterSide;
   int targetHex;
   hero* casterHero;
   short originalMana;
   int originalCastFlag;
};

PendingExternalCombatTransaction pendingExternalCombatTransactions[8] = {};

ExternalSpellSlot* getExternalSpellSlot(const int spellId)
{
   if (spellId < NEWSPELLS_EXTERNAL_SPELL_FIRST_ID ||
       spellId > NEWSPELLS_EXTERNAL_SPELL_LAST_ID)
      return 0;
   return &externalSpellSlots[spellId - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID];
}

bool copyProviderKey(char* const destination, const std::size_t capacity,
                     const char* const source)
{
   if (!destination || capacity < 2 || !source)
      return false;

   std::size_t length = 0;
   __try
   {
      while (length < capacity && source[length])
      {
         const unsigned char ch = static_cast<unsigned char>(source[length]);
         if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
               (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' ||
               ch == '-'))
            return false;
         ++length;
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      return false;
   }

   if (!length || length >= capacity)
      return false;
   memcpy(destination, source, length + 1);
   return true;
}

bool isExecutableCallback(const void* const callback)
{
   if (!callback)
      return false;

   MEMORY_BASIC_INFORMATION info = {};
   if (!VirtualQuery(callback, &info, sizeof(info)) ||
       info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) ||
       (info.Protect & PAGE_NOACCESS))
      return false;

   const DWORD protection = info.Protect & 0xFF;
   return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
      protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

bool validateProviderCallbacks(const NewSpellsProviderSpellV1& spell)
{
   struct CallbackRequirement
   {
      std::uint32_t capability;
      const void* callback;
   };

   const CallbackRequirement requirements[] =
   {
      {NEWSPELLS_CAP_ADVENTURE_CAST,
       reinterpret_cast<const void*>(spell.CastAdventure)},
      {NEWSPELLS_CAP_COMBAT_TARGET,
       reinterpret_cast<const void*>(spell.ValidateCombatTarget)},
      {NEWSPELLS_CAP_COMBAT_CAST,
       reinterpret_cast<const void*>(spell.CastCombat)},
      {NEWSPELLS_CAP_STATUS_APPLY,
       reinterpret_cast<const void*>(spell.OnStatusApply)},
      {NEWSPELLS_CAP_STATUS_ROUND,
       reinterpret_cast<const void*>(spell.OnStatusRound)},
      {NEWSPELLS_CAP_STATUS_REMOVE,
       reinterpret_cast<const void*>(spell.OnStatusRemove)},
      {NEWSPELLS_CAP_CURE_DISPEL,
       reinterpret_cast<const void*>(spell.OnCureOrDispel)},
      {NEWSPELLS_CAP_BATTLE_LIFECYCLE,
       reinterpret_cast<const void*>(spell.OnBattleLifecycle)},
      {NEWSPELLS_CAP_CREATURE_CAST,
       reinterpret_cast<const void*>(spell.OnCreatureCast)},
      {NEWSPELLS_CAP_ERM_CAST,
       reinterpret_cast<const void*>(spell.OnErmCast)},
      {NEWSPELLS_CAP_COMBAT_AI,
       reinterpret_cast<const void*>(spell.EvaluateCombatAi)},
      {NEWSPELLS_CAP_ADVENTURE_AI,
       reinterpret_cast<const void*>(spell.EvaluateAdventureAi)}
   };

   if (!spell.capabilities ||
       (spell.capabilities & ~NEWSPELLS_CAP_ALL_V1) ||
       (spell.capabilities & ~providerRuntimeCapabilities) ||
       (spell.flags & ~NEWSPELLS_PROVIDER_HUMAN_ONLY))
      return false;

   for (std::size_t i = 0; i < sizeof(requirements) / sizeof(requirements[0]); ++i)
   {
      const bool declared = (spell.capabilities & requirements[i].capability) != 0;
      if (declared != (requirements[i].callback != 0) ||
          (declared && !isExecutableCallback(requirements[i].callback)))
         return false;
   }

   if (spell.ValidateAdventure &&
       !isExecutableCallback(reinterpret_cast<const void*>(spell.ValidateAdventure)))
      return false;

   if ((spell.flags & NEWSPELLS_PROVIDER_HUMAN_ONLY) &&
       (spell.capabilities &
        (NEWSPELLS_CAP_COMBAT_AI | NEWSPELLS_CAP_ADVENTURE_AI)))
      return false;
   if ((spell.capabilities & NEWSPELLS_CAP_COMBAT_TARGET) &&
       !(spell.capabilities & NEWSPELLS_CAP_COMBAT_CAST))
      return false;
   if ((spell.capabilities & NEWSPELLS_CAP_COMBAT_AI) &&
       !(spell.capabilities & NEWSPELLS_CAP_COMBAT_CAST))
      return false;
   if ((spell.capabilities & NEWSPELLS_CAP_ADVENTURE_AI) &&
       !(spell.capabilities & NEWSPELLS_CAP_ADVENTURE_CAST))
      return false;
   if (spell.ValidateAdventure &&
       !(spell.capabilities & NEWSPELLS_CAP_ADVENTURE_CAST))
      return false;

   return true;
}

int32_t __stdcall registerProviderBatch(
   const NewSpellsProviderBatchV1* const batch)
{
   if (!providerRegistrationOpen || !batch)
      return 0;

   char batchProviderKey[64] = {};
   __try
   {
      if (batch->size < sizeof(NewSpellsProviderBatchV1) ||
          batch->abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
          !batch->spells || !batch->spellCount ||
          batch->spellCount > static_cast<std::uint32_t>(
             NEWSPELLS_EXTERNAL_SPELL_LAST_ID -
             NEWSPELLS_EXTERNAL_SPELL_FIRST_ID + 1) ||
          !copyProviderKey(batchProviderKey, sizeof(batchProviderKey),
                           batch->providerKey))
         return 0;

      bool duplicateProvider = false;
      for (int id = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
           id <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++id)
      {
         ExternalSpellSlot* const existing = getExternalSpellSlot(id);
         if (existing->registered &&
             strcmp(existing->providerKey, batchProviderKey) == 0)
         {
            existing->contested = true;
            duplicateProvider = true;
         }
      }
      if (duplicateProvider)
         return 0;

      for (std::uint32_t i = 0; i < batch->spellCount; ++i)
      {
         const NewSpellsProviderSpellV1& spell = batch->spells[i];
         char providerKey[64] = {};
         char spellKey[64] = {};
         if (spell.size < sizeof(NewSpellsProviderSpellV1) ||
             spell.abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
             !copyProviderKey(providerKey, sizeof(providerKey), spell.providerKey) ||
             strcmp(providerKey, batchProviderKey) != 0 ||
             !copyProviderKey(spellKey, sizeof(spellKey), spell.spellKey) ||
             !validateProviderCallbacks(spell))
            return 0;

         ExternalSpellSlot* const slot = getExternalSpellSlot(spell.spellId);
         if (!slot)
            return 0;
         if (slot->registered)
         {
            slot->contested = true;
            return 0;
         }

         for (std::uint32_t previous = 0; previous < i; ++previous)
            if (batch->spells[previous].spellId == spell.spellId ||
                strcmp(batch->spells[previous].spellKey, spell.spellKey) == 0)
               return 0;
      }

      for (std::uint32_t i = 0; i < batch->spellCount; ++i)
      {
         const NewSpellsProviderSpellV1& spell = batch->spells[i];
         ExternalSpellSlot* const slot = getExternalSpellSlot(spell.spellId);
         memset(slot, 0, sizeof(*slot));
         slot->descriptor = spell;
         copyProviderKey(slot->providerKey, sizeof(slot->providerKey),
                         spell.providerKey);
         copyProviderKey(slot->spellKey, sizeof(slot->spellKey), spell.spellKey);
         slot->descriptor.providerKey = slot->providerKey;
         slot->descriptor.spellKey = slot->spellKey;
         slot->registered = true;
      }
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      return 0;
   }

   return 1;
}

int32_t __stdcall isProviderRegistrationOpen()
{
   return providerRegistrationOpen ? 1 : 0;
}

int32_t __stdcall isProviderSpellRegistered(const int32_t spellId)
{
   const ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
   if (!slot)
      return 0;
   return providerRegistrationOpen
      ? (slot->registered && !slot->contested ? 1 : 0)
      : (slot->active && !slot->faulted ? 1 : 0);
}

NewSpellsProviderRegistryV1 newSpellsProviderRegistryV1 =
{
   sizeof(NewSpellsProviderRegistryV1),
   NEWSPELLS_PROVIDER_ABI_VERSION_V1,
   NEWSPELLS_CAP_ALL_V1,
   registerProviderBatch,
   isProviderRegistrationOpen,
   isProviderSpellRegistered
};

bool publishSpellProviderRegistry()
{
   if (_P->VarFind(SPELL_PROVIDER_REGISTRY_VARIABLE_NAME))
      return false;

   const _dword_ registryAddress = static_cast<_dword_>(
      reinterpret_cast<std::uintptr_t>(&newSpellsProviderRegistryV1));
   Variable* const variable = _P->VarInit(
      SPELL_PROVIDER_REGISTRY_VARIABLE_NAME, registryAddress);
   return variable && variable->GetValue() == registryAddress;
}

void failExternalSpellClosed(ExternalSpellSlot& slot)
{
   slot.faulted = true;
   slot.active = false;
   if (pGame)
      pGame->DisableSpell(static_cast<SpellID>(slot.descriptor.spellId));

   const int spellId = slot.descriptor.spellId;
   if (!pCombatManager || spellId < NEWSPELLS_EXTERNAL_SPELL_FIRST_ID ||
       spellId > NEWSPELLS_EXTERNAL_SPELL_LAST_ID)
      return;

   for (int side = ATTACKER; side <= DEFENDER; ++side)
      for (int index = 0; index < 21; ++index)
      {
         army* const Army = reinterpret_cast<army*>(
            &pCombatManager->stack[side][index]);
         if (!hasValidArmyCoordinates(Army) ||
             !nsDuration(Army, spellId))
            continue;
         // Use the engine remover after faulting/deactivating the slot.  A
         // direct zero would leave the spell ID in SpellInfluenceQueue and
         // corrupt later round processing.  Since the slot is already
         // inactive, neither the dispel pre-hook nor resetSpell can call the
         // failed provider again.
         Army->CancelIndividualSpell(spellId);
         activeSpellMastery[side][index][spellId] = eMasteryNone;
      }
}

bool isKnownProviderResult(const int32_t result)
{
   return result >= NEWSPELLS_PROVIDER_UNSUPPORTED &&
      result <= NEWSPELLS_PROVIDER_COMMITTED;
}

int32_t invokeAdventureCallback(ExternalSpellSlot& slot,
   NewSpellsAdventureCallbackV1 callback, NewSpellsAdventureContextV1& context)
{
   if (!callback || slot.faulted)
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   int32_t result = NEWSPELLS_PROVIDER_UNSUPPORTED;
   __try
   {
      result = callback(&context);
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   if (!isKnownProviderResult(result))
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   return result;
}

int32_t invokeCombatCallback(ExternalSpellSlot& slot,
   NewSpellsCombatCallbackV1 callback, NewSpellsCombatContextV1& context)
{
   if (!callback || slot.faulted)
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   int32_t result = NEWSPELLS_PROVIDER_UNSUPPORTED;
   __try
   {
      result = callback(&context);
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   if (!isKnownProviderResult(result))
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   return result;
}

int32_t invokeStatusCallback(ExternalSpellSlot& slot,
   NewSpellsStatusCallbackV1 callback, NewSpellsStatusContextV1& context)
{
   if (!callback || slot.faulted)
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   int32_t result = NEWSPELLS_PROVIDER_UNSUPPORTED;
   __try
   {
      result = callback(&context);
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   if (!isKnownProviderResult(result))
   {
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   return result;
}

int32_t invokeBattleCallback(ExternalSpellSlot& slot,
   NewSpellsBattleCallbackV1 callback, NewSpellsBattleContextV1& context)
{
   if (!callback || slot.faulted)
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   int32_t result = NEWSPELLS_PROVIDER_UNSUPPORTED;
   __try
   {
      result = callback(&context);
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   if (!isKnownProviderResult(result))
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   return result;
}

int32_t invokeAiCallback(ExternalSpellSlot& slot,
   NewSpellsAiCallbackV1 callback, NewSpellsAiContextV1& context)
{
   if (!callback || slot.faulted)
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   int32_t result = NEWSPELLS_PROVIDER_UNSUPPORTED;
   __try
   {
      result = callback(&context);
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   if (!isKnownProviderResult(result))
   {
      failExternalSpellClosed(slot);
      return NEWSPELLS_PROVIDER_UNSUPPORTED;
   }
   return result;
}

PendingExternalCombatTransaction* findPendingExternalCombatTransaction(
   const std::uintptr_t frame, const int spellId = ID_NONE)
{
   for (std::size_t i = 0; i < sizeof(pendingExternalCombatTransactions) /
        sizeof(pendingExternalCombatTransactions[0]); ++i)
   {
      PendingExternalCombatTransaction& pending =
         pendingExternalCombatTransactions[i];
      if (pending.valid && pending.frame == frame &&
          (spellId == ID_NONE || pending.spellId == spellId))
         return &pending;
   }
   return 0;
}

PendingExternalCombatTransaction* findPendingExternalCombatTransaction(
   const int spellId, const int casterSide, const int targetHex)
{
   for (std::size_t i = 0; i < sizeof(pendingExternalCombatTransactions) /
        sizeof(pendingExternalCombatTransactions[0]); ++i)
   {
      PendingExternalCombatTransaction& pending =
         pendingExternalCombatTransactions[i];
      if (pending.valid && pending.spellId == spellId &&
          pending.casterSide == casterSide &&
          (targetHex == ID_NONE || pending.targetHex == targetHex))
         return &pending;
   }
   return 0;
}

PendingExternalCombatTransaction* beginExternalCombatTransaction(
   HookContext* const c, const int spellId)
{
   PendingExternalCombatTransaction* pending =
      findPendingExternalCombatTransaction(c->ebp);
   if (!pending)
   {
      for (std::size_t i = 0; i < sizeof(pendingExternalCombatTransactions) /
           sizeof(pendingExternalCombatTransactions[0]); ++i)
         if (!pendingExternalCombatTransactions[i].valid)
         {
            pending = &pendingExternalCombatTransactions[i];
            break;
         }
   }
   // Never overwrite an in-flight transaction. A provider can re-enter the
   // native casting path, and corrupting an older frame would make the later
   // epilogue restore the wrong hero's mana/cast counter.
   if (!pending)
      return 0;

   memset(pending, 0, sizeof(*pending));
   pending->valid = true;
   pending->frame = c->ebp;
   pending->spellId = spellId;
   pending->casterKind = *reinterpret_cast<int*>(c->ebp + 0x10);
   pending->casterSide = pCombatManager ? pCombatManager->current_side : ID_NONE;
   pending->targetHex = *reinterpret_cast<int*>(c->ebp + 0x0C);
   pending->casterHero = *reinterpret_cast<hero**>(c->ebp - 0x14);
   if (pending->casterKind == NEWSPELLS_SOURCE_HERO &&
       pending->casterHero && pCombatManager &&
       pending->casterSide >= ATTACKER && pending->casterSide <= DEFENDER)
   {
      pending->originalMana = pending->casterHero->mana;
      pending->originalCastFlag = pCombatManager->Field<int>(
         0x54B4 + pending->casterSide * sizeof(int));
   }
   return pending;
}

void cancelExternalCombatTransaction(HookContext* const c,
   PendingExternalCombatTransaction* const pending)
{
   const int casterKind = *reinterpret_cast<int*>(c->ebp + 0x10);
   if (casterKind == NEWSPELLS_SOURCE_HERO)
   {
      hero* const Hero = *reinterpret_cast<hero**>(c->ebp - 0x14);
      const int side = pCombatManager ? pCombatManager->current_side : ID_NONE;
      if (pending && pending->casterHero == Hero)
      {
         if (Hero)
            Hero->mana = pending->originalMana;
         if (pCombatManager && side >= ATTACKER && side <= DEFENDER)
            pCombatManager->Field<int>(0x54B4 + side * sizeof(int)) =
               pending->originalCastFlag;
      }
      else
      {
         const int chargedMana = *reinterpret_cast<int*>(c->ebp - 0x68);
         if (Hero && chargedMana > 0 && chargedMana <= 32767)
            Hero->mana = static_cast<short>(Hero->mana + chargedMana);
         if (pCombatManager && side >= ATTACKER && side <= DEFENDER)
            pCombatManager->Field<int>(0x54B4 + side * sizeof(int)) = 0;
      }
   }

   // Prevent the cancellation cleanup from granting Magic Channel mana based
   // on a spell which the provider did not commit.
   *reinterpret_cast<int*>(c->ebp - 0x68) = 0;
   if (pending)
      pending->valid = false;

   // Starts at the hero-animation/action refresh portion of CastSpell's tail,
   // bypassing the normal committed-effect epilogue at 0x5A2368.
   c->return_address = 0x5A23B8;
}

int __stdcall clearCompletedExternalCombatTransaction(LoHook* h,
                                                       HookContext* c)
{
   PendingExternalCombatTransaction* const pending =
      findPendingExternalCombatTransaction(c->ebp);
   if (pending)
      pending->valid = false;
   return EXEC_DEFAULT;
}

void loadMapDisabledSpells()
{
   NewSpellsMap::Clear(mapDisabledSpells);

   char mapFileName[MAX_PATH] = {};
   Era::GetMapFileName(mapFileName);

   if (*mapFileName &&
       !NewSpellsMap::ReadTrailerFileA(mapFileName, mapDisabledSpells))
   {
      const bool absolutePath =
         (mapFileName[0] && mapFileName[1] == ':') ||
         (mapFileName[0] == '\\' && mapFileName[1] == '\\');
      const bool alreadyInMaps = _strnicmp(mapFileName, "Maps\\", 5) == 0;
      if (!absolutePath && !alreadyInMaps)
      {
         char mapsPath[MAX_PATH];
         if (sprintf_s(mapsPath, sizeof(mapsPath),
                       "Maps\\%s", mapFileName) >= 0)
            NewSpellsMap::ReadTrailerFileA(mapsPath, mapDisabledSpells);
      }
   }
}

void applySavedMapDisabledSpells()
{
   if (!pGame)
      return;

   for (int spellId = 0; spellId < SPELLS_MAX; ++spellId)
      if (NewSpellsMap::IsDisabled(mapDisabledSpells, spellId))
         pGame->DisableSpell(static_cast<SpellID>(spellId));
}

void __stdcall saveMapDisabledSpells(Era::TEvent* Event)
{
   NewSpellsMap::Trailer trailer;
   NewSpellsMap::BuildTrailer(mapDisabledSpells, trailer);
   Era::WriteSavegameSection(sizeof(trailer), &trailer,
      MAP_DISABLED_SAVE_SECTION);
}

void __stdcall loadSavedMapDisabledSpells(Era::TEvent* Event)
{
   NewSpellsMap::Trailer trailer;
   const int bytesRead = Era::ReadSavegameSection(sizeof(trailer), &trailer,
      MAP_DISABLED_SAVE_SECTION);
   if (bytesRead != static_cast<int>(sizeof(trailer)) ||
       !NewSpellsMap::ValidateTrailer(trailer))
      return;

   std::memcpy(mapDisabledSpells.disabled, trailer.disabled,
      sizeof(mapDisabledSpells.disabled));
   applySavedMapDisabledSpells();
}

// These are not exactly optional tables, so don't remove them
char spellIndirectTableA[SPELLS_MAX - SPELL_QUICKSAND] =
{
   // Spells (starting from Quicksand #10)
    0,  1,  2,  2,  3,  4,  4,  4,  4,  4,
    5,  5,  5,  5,  4,  4,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    6,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,  4,  4,  4,  4,  4,  4,  4,  4,  5,
    4,  4,  4,  7,  8,  9, 10, 10, 10, 10,
   // Special Abilities (starting from Stone Gaze #71)
    4,  4,  4,  4,  4,  4,  4,  4,  4,  4,
    4,
   // New Spells (starting from Fear #81)
    4,  5,  4,  4, 10, 10, 10,  4,  4,  4,
	4,  5,  4,  4,  4,  4
};

char spellIndirectTableB[SPELLS_MAX - SPELL_QUICKSAND] =
{
   // Spells (starting from Quicksand #10)
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
   10, 11, 12, 13, 14, 15, 16, 17, 17, 17,
   17, 17, 17, 17, 17, 18, 17, 19, 20, 20,
   21, 17, 17, 22, 17, 17, 17, 23, 17, 17,
   17, 17, 17, 17, 17, 17, 17,  7, 17, 24,
   17, 17, 17, 25, 26, 27, 28, 29, 30, 31,
   // Special Abilities (starting from Stone Gaze #71)
   32, 33, 34, 17, 17, 34, 37, 37, 35, 37,
   36,
   // New Spells (starting from Fear #81)
   17, 11, 17, 17, 38, 39, 40, 17, 17, 17,
   17, 11, 17, 17, 17, 17
};

char SetSpellInfluenceTable[SPELLS_MAX - SPELL_SHIELD] =
{
   // Spells (starting from Shield #27)
    0,  1,  2,  3,  4,  5,  6,  7, 32,  8,
   32, 32, 32, 32,  9, 10, 11, 12, 13, 14,
   32, 15, 16, 17, 18, 19, 20, 21, 22, 23,
   32, 24, 25, 26, 27, 28, 32, 32, 32, 32,
   32, 32, 32,
   // Special Abilities (starting from Stone Gaze #71)
   32, 29, 32, 30, 32, 31, 32, 32, 32, 32,
   32,
   // New Spells (starting from Fear #81)
   32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   32, 32, 32, 32, 32, 32
};

char spellIndirectTableD[SPELLS_MAX - SPELL_WEAKNESS] =
{
   // Spells (starting from Weakness #45)
    0,  1,  9,  2,  9,  9,  9,  9,  3,  4,
    9,  9,  9,  9,  9,  5,  9,  9,  9,  9,
    9,  9,  9,  9,  9,
    // Special Abilities (starting from Stone Gaze #71)
    9,  9,  6,  7,  9,  8,  9,  9,  9,  9,
    9,
    // New Spells (starting from Fear #81)
    9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
	9,  9,  9,  9,  9,  9
};

char spellIndirectTableE[SPELLS_MAX - SPELL_LIGHTNING_BOLT] =
{
   // Spells (starting from Lightning Bolt #17)
    0, 16,  0, 16, 16, 16, 16,  1,  2, 16,
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
   16,  3,  4, 16,  5,  6, 16,  7, 16, 16,
   16, 16,  8,  8,  9,  9, 16, 16,  9, 16,
   16, 16, 10, 11, 12, 13, 16, 16, 16, 16,
   16, 16, 16,
   // Special Abilities (starting from Stone Gaze #71)
   14, 15, 16, 16, 16, 16, 16, 16, 16, 16,
   16,
   // New Spells (starting from Fear #81)
   16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
   16, 16, 16, 16, 16, 16
};

char spellIndirectTableF[SPELLS_MAX - SPELL_EARTHQUAKE] =
{
   // Spells (starting from Earthquake #14)
    0,  9,  9,  9,  9,  1,  2,  2,  2,  2,
    3,  3,  3,  9,  9,  9,  9,  9,  9,  9,
    9,  4,  9,  9,  5,  5,  6,  9,  9,  9,
    9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
    9,  9,  9,  9,  9,  9,  9,  9,  9,  7,
    9,  9,  8,  8,  8,  8,
   // Special Abilities (starting from Stone Gaze #71)
    9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
    9,
   // New Spells (starting from Fear #81)
    9,  2,  9,  9,  8,  8,  8,  9,  9,  9,
	9,  2,  9,  9,  9,  9
};

void initializeIndirectTableTails()
{
   memset(spellIndirectTableA + DEFAULT_SPELLS_NUM - SPELL_QUICKSAND, 4,
      sizeof(spellIndirectTableA) - (DEFAULT_SPELLS_NUM - SPELL_QUICKSAND));
   memset(spellIndirectTableB + DEFAULT_SPELLS_NUM - SPELL_QUICKSAND, 17,
      sizeof(spellIndirectTableB) - (DEFAULT_SPELLS_NUM - SPELL_QUICKSAND));
   memset(SetSpellInfluenceTable + DEFAULT_SPELLS_NUM - SPELL_SHIELD, 32,
      sizeof(SetSpellInfluenceTable) - (DEFAULT_SPELLS_NUM - SPELL_SHIELD));
   memset(spellIndirectTableD + DEFAULT_SPELLS_NUM - SPELL_WEAKNESS, 9,
      sizeof(spellIndirectTableD) - (DEFAULT_SPELLS_NUM - SPELL_WEAKNESS));
   memset(spellIndirectTableE + DEFAULT_SPELLS_NUM - SPELL_LIGHTNING_BOLT, 16,
      sizeof(spellIndirectTableE) - (DEFAULT_SPELLS_NUM - SPELL_LIGHTNING_BOLT));
   memset(spellIndirectTableF + DEFAULT_SPELLS_NUM - SPELL_EARTHQUAKE, 9,
      sizeof(spellIndirectTableF) - (DEFAULT_SPELLS_NUM - SPELL_EARTHQUAKE));
}

bool shrineSpells[SPELLS_MAX];
int activeSpellMastery[2][21][SPELLS_MAX];
int origNumTroops[2][21];

// Battle Spells
struct SlowSpell
{
   int speedMod;
}
slowSpell[2][21];

struct FearSpell
{
   int speedMod;
   int retalDamageMod;
}
fearSpellParams[4], fearSpell[2][21];

struct PoisonSpell
{
   float healthModFirstRound;
   float healthMod;
   float minHealth;
}
poisonSpellParams[4], poisonSpell[2][21];

struct DiseaseSpell
{
   int attackPenalty;
   int defensePenalty;
   int speedMod;
}
diseaseSpellParams[4], diseaseSpell[2][21];

struct AgeSpell
{
   int healthMod;
}
ageSpell[2][21];

struct ToughnessSpell
{
   int healthMod;
}
toughnessSpell[2][21];

struct ExplosionSpell
{
   bool speedPenalty;
}
explosionSpell[2][21];

struct HourOfPowerSpell
{
   int healthMod;
}
hourOfPowerSpell[2][21];

struct GoldenTouchSpell
{
   int goldForCast;
   int goldForBattle;
}
goldenTouchSpell[2];

// Adventure Spells
struct HeroAdvInfoEx
{
   char mobilityCastCount;
   bool mobilityWonBattle;
   char eyeOfTheMagiCastCount;
}
heroAdvInfoEx[HEROES_NUM];

bool mobilityRequiresBattle;
bool explosionSpeedReduction;
bool incinerationSpecialFeature;
char* mobilityWinBattleMessage;
char* goldenTouchCombatLogMessage;
char* goldenTouchWinBattleMessage;
char emptyLocalizedText[] = "";

char* getLocalizedText(const std::string& key)
{
   char* value = Era::tr(key.c_str());
   return value && strcmp(value, key.c_str()) != 0 ? value : emptyLocalizedText;
}

void playSound(const char* fileName)
{
    CALL_1(_Sample_, __fastcall, 0x59A770, fileName);
}

int forceCappedDuration[SPELLS_MAX];

#include "NsHeroSpells.h"
#include "NsDisabledSpells.h"
#include "NsDurations.h"
#include "NsSpellBounds.h"
#include "NsVirtualLod.h"

// Game Bug Fixes Extended owns the six-byte instruction at 0x56B344 in ERA
// to prevent AI Town Portal on cursed ground. The old NewSpells code rewrote
// that instruction's displacement at 0x56B346, corrupting whichever patch was
// installed first. Hook the following TEST instruction instead and replace AL
// with the value from the expanded spellbook without touching the other hook.
int __stdcall aiTownPortalExpandedSpellbook(LoHook* h, HookContext* c)
{
   hero* Hero = reinterpret_cast<hero*>(c->esi);
   const unsigned char hasTownPortal = Hero ? heroAvailableSpell(Hero, SPELL_TOWN_PORTAL) : 0;

   c->eax = (c->eax & ~0xFF) | hasTownPortal;
   return EXEC_DEFAULT;
}

int clampConfigInt(int value, int minimum, int maximum)
{
   return max(min(value, maximum), minimum);
}

bool tryGetJsonValue(const std::string& key, char*& value)
{
   value = Era::tr(key.c_str());
   return value && strcmp(value, key.c_str()) != 0;
}

bool tryGetJsonInt(const std::string& key, int& value)
{
   char* text = 0;
   if (!tryGetJsonValue(key, text) || !*text)
      return false;

   char* end = 0;
   const long parsed = strtol(text, &end, 10);
   while (end && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
      ++end;

   if (!end || end == text || *end != '\0' ||
       parsed < (std::numeric_limits<int>::min)() ||
       parsed > (std::numeric_limits<int>::max)())
      return false;

   value = static_cast<int>(parsed);
   return true;
}

int getJsonInt(const std::string& key, const int defaultValue)
{
   int value = defaultValue;
   tryGetJsonInt(key, value);
   return value;
}

char* getJsonString(const std::string& key, char* defaultValue)
{
   char* value = 0;
   return tryGetJsonValue(key, value) ? value : defaultValue;
}

#include "NsDataSpells.h"
#include "NsEvents.h"
#include "NsOptions.h"

std::string makeSpellJsonKey(const int spellId, const char* field,
                             const int index = ID_NONE)
{
   char key[96];
   if (index == ID_NONE)
      sprintf_s(key, sizeof(key), "era.spells.%d.%s", spellId, field);
   else
      sprintf_s(key, sizeof(key), "era.spells.%d.%s.%d", spellId, field, index);
   return key;
}

std::string makePrivateSpellJsonKey(const int spellId, const char* field)
{
   char key[96];
   sprintf_s(key, sizeof(key), "NewSpells.Spells.%d.%s", spellId, field);
   return key;
}

std::string makeExternalSpellJsonKey(const int spellId, const char* field)
{
   if (nsDataSpell(spellId))
      return nsDataKey(spellId, field);
   char key[128];
   sprintf_s(key, sizeof(key), "NewSpells.ExternalSpells.%d.%s", spellId,
      field);
   return key;
}

bool hasValidStandardExternalSpellJson(const int spellId,
                                       const char* const kind)
{
   char* name = 0;
   char* shortName = 0;
   char* soundName = 0;
   int type = 0;
   int animationIndex = 0;
   int level = 0;
   int school = 0;
   int spellEffect = 0;
   int flags = 0;
   if (!tryGetJsonInt(makeSpellJsonKey(spellId, "type"), type) ||
       type < -128 || type > 127 ||
       !tryGetJsonValue(makeSpellJsonKey(spellId, "soundName"), soundName) ||
       !soundName ||
       !tryGetJsonInt(makeSpellJsonKey(spellId, "animationIndex"),
                      animationIndex) ||
       // Numeric indices are limited to the stable native table. External and
       // core-added animations must be selected by a namespaced animationKey.
       animationIndex < 0 || animationIndex >= ANIM_FEAR ||
       !tryGetJsonValue(makeSpellJsonKey(spellId, "name"), name) || !name ||
       !*name ||
       !tryGetJsonValue(makeSpellJsonKey(spellId, "shortName"), shortName) ||
       !shortName || !*shortName ||
       !tryGetJsonInt(makeSpellJsonKey(spellId, "level"), level) ||
       level < 1 || level > 5 ||
       !tryGetJsonInt(makeSpellJsonKey(spellId, "flags"), flags) ||
       flags < 0 ||
       (static_cast<std::uint32_t>(flags) & ~0x001FFFFFu) ||
       (flags & SF_AI_ADVENTUREMAP) ||
       !tryGetJsonInt(makeSpellJsonKey(spellId, "school"), school) ||
       school < 0 || (school & ~0xF) ||
       !tryGetJsonInt(makeSpellJsonKey(spellId, "spEffect"), spellEffect) ||
       spellEffect < -1000000 || spellEffect > 1000000)
      return false;

   for (int mastery = eMasteryNone; mastery <= eMasteryExpert; ++mastery)
   {
      int mana = 0;
      int effect = 0;
      int ai = 0;
      char* description = 0;
      if (!tryGetJsonInt(makeSpellJsonKey(spellId, "manaCost", mastery),
                         mana) ||
          mana < 0 || mana > 32767 ||
          !tryGetJsonInt(makeSpellJsonKey(spellId, "baseValue", mastery),
                         effect) ||
          effect < -1000000 || effect > 1000000 ||
          !tryGetJsonInt(makeSpellJsonKey(spellId, "aiValue", mastery), ai) ||
          ai < 0 || ai > 1000000000 ||
          !tryGetJsonValue(makeSpellJsonKey(spellId, "description", mastery),
                           description) ||
          !description || !*description)
         return false;
   }

   for (int town = CASTLE; town <= CONFLUX; ++town)
   {
      int probability = 0;
      if (!tryGetJsonInt(makeSpellJsonKey(spellId, "chanceToGet", town),
                         probability) ||
          probability < 0 || probability > 100)
         return false;
   }

   const bool isAdventure = (flags & SF_MAP_SPELL) != 0;
   const bool isCombat = (flags & SF_BATTLE_SPELL) != 0;
   if (strcmp(kind, "adventure") == 0)
      return isAdventure && !isCombat;
   if (strcmp(kind, "combat") == 0)
      return isCombat && !isAdventure;
   if (strcmp(kind, "hybrid") == 0)
      return isAdventure && isCombat;
   return false;
}

bool getExternalCombatTargetDispatch(const int spellId,
                                     const char* const kind,
                                     unsigned char& dispatch)
{
   dispatch = 4;
   char* configuredMode = 0;
   const bool hasConfiguredMode = tryGetJsonValue(
      makeExternalSpellJsonKey(spellId, "combatTargetMode"), configuredMode);

   if (!kind)
      return false;
   if (strcmp(kind, "adventure") == 0)
      return !hasConfiguredMode;

   int flags = 0;
   if (!tryGetJsonInt(makeSpellJsonKey(spellId, "flags"), flags))
      return false;

   ExternalCombatTargetMode inferredMode = EXTERNAL_TARGET_MODE_GLOBAL;
   if (flags & SF_SINGLE_TARGET)
      inferredMode = EXTERNAL_TARGET_MODE_TARGETED;
   else if (flags & (SF_TARGET_ANYWHERE | SF_AI_AREA_EFFECT))
      inferredMode = EXTERNAL_TARGET_MODE_AREA;
   else if (flags & SF_AI_CREATURES)
      inferredMode = EXTERNAL_TARGET_MODE_SUMMON;

   ExternalCombatTargetMode selectedMode = inferredMode;
   if (hasConfiguredMode)
   {
      if (!configuredMode || !*configuredMode)
         return false;
      if (strcmp(configuredMode, "targeted") == 0)
         selectedMode = EXTERNAL_TARGET_MODE_TARGETED;
      else if (strcmp(configuredMode, "area") == 0)
         selectedMode = EXTERNAL_TARGET_MODE_AREA;
      else if (strcmp(configuredMode, "global") == 0)
         selectedMode = EXTERNAL_TARGET_MODE_GLOBAL;
      else if (strcmp(configuredMode, "summon") == 0)
         selectedMode = EXTERNAL_TARGET_MODE_SUMMON;
      else
         return false;

      // The standard flags and targeting declaration describe the same UI
      // contract. Reject disagreement instead of routing through an unrelated
      // native targeting branch.
      if (selectedMode != inferredMode)
         return false;
   }

   const bool singleTarget = (flags & SF_SINGLE_TARGET) != 0;
   const bool areaTarget =
      (flags & (SF_TARGET_ANYWHERE | SF_AI_AREA_EFFECT)) != 0;
   const bool summonsCreatures = (flags & SF_AI_CREATURES) != 0;
   switch (selectedMode)
   {
   case EXTERNAL_TARGET_MODE_TARGETED:
      if (!singleTarget || areaTarget || summonsCreatures)
         return false;
      break;
   case EXTERNAL_TARGET_MODE_AREA:
      if (singleTarget || !areaTarget || summonsCreatures)
         return false;
      break;
   case EXTERNAL_TARGET_MODE_GLOBAL:
      if (singleTarget || areaTarget || summonsCreatures)
         return false;
      break;
   case EXTERNAL_TARGET_MODE_SUMMON:
      if (singleTarget || areaTarget || !summonsCreatures)
         return false;
      break;
   default:
      return false;
   }

   // InitiateSpell table-A case 10 is elemental-specific and rejects unknown
   // spell IDs. Provider summons therefore use the neutral immediate A=4 path,
   // just like vanilla global spells; providers own summon validation/state.
   dispatch = selectedMode == EXTERNAL_TARGET_MODE_AREA ? 5 : 4;
   return true;
}

bool getExternalSpellDeclaration(const int spellId, char*& providerKey,
                                 char*& spellKey, char*& kind,
                                 int& capabilities)
{
   providerKey = spellKey = kind = 0;
   capabilities = 0;
   int editorVisible = 0;
   if (nsDataSpellDeclaration(spellId, providerKey, spellKey, kind, capabilities))
   {
      editorVisible = nsDataInt(spellId, "editorVisible", 1);
      if (editorVisible != 0 && editorVisible != 1)
         return false;
   }
   else if (!tryGetJsonValue(makeExternalSpellJsonKey(spellId, "provider"),
                        providerKey) ||
       !tryGetJsonValue(makeExternalSpellJsonKey(spellId, "spellKey"),
                        spellKey) ||
       !tryGetJsonValue(makeExternalSpellJsonKey(spellId, "kind"), kind) ||
       !tryGetJsonInt(makeExternalSpellJsonKey(spellId, "editorVisible"),
                      editorVisible) ||
       (editorVisible != 0 && editorVisible != 1) ||
       !tryGetJsonInt(makeExternalSpellJsonKey(spellId, "capabilities"),
                      capabilities) ||
       capabilities <= 0 ||
       (static_cast<std::uint32_t>(capabilities) &
          ~NEWSPELLS_CAP_ALL_V1))
      return false;

   char checkedProvider[64] = {};
   char checkedSpell[64] = {};
   char* animationDef = 0;
   char* animationName = 0;
   char* animationKey = 0;
   int animationType = 0;
   const bool hasAnimationDef = tryGetJsonValue(makeExternalSpellJsonKey(
      spellId, "animationDef"), animationDef);
   const bool hasAnimationName = tryGetJsonValue(makeExternalSpellJsonKey(
      spellId, "animationName"), animationName);
   const bool hasAnimationType = tryGetJsonInt(makeExternalSpellJsonKey(
      spellId, "animationType"), animationType);
   const bool hasAnimationKey = tryGetJsonValue(makeExternalSpellJsonKey(
      spellId, "animationKey"), animationKey);

   if (!copyProviderKey(checkedProvider, sizeof(checkedProvider), providerKey) ||
       !copyProviderKey(checkedSpell, sizeof(checkedSpell), spellKey))
      return false;

   const bool hasCustomAnimation = hasAnimationDef || hasAnimationName ||
      hasAnimationType;
   if (hasCustomAnimation)
   {
      char checkedDef[64] = {};
      char checkedName[64] = {};
      char namespacedKey[192] = {};
      if (!hasAnimationDef || !hasAnimationName || !hasAnimationType ||
          animationType < 0 || animationType > 0xFFFF ||
          !copyProviderKey(checkedDef, sizeof(checkedDef), animationDef) ||
          !copyProviderKey(checkedName, sizeof(checkedName), animationName) ||
          sprintf_s(namespacedKey, sizeof(namespacedKey), "%s.%s.%s",
             checkedProvider, checkedSpell, checkedName) < 0 ||
          (hasAnimationKey &&
           (!animationKey || strcmp(animationKey, namespacedKey) != 0)))
         return false;
   }
   else if (hasAnimationKey)
   {
      char checkedKey[192] = {};
      char providerPrefix[72] = {};
      if (!copyProviderKey(checkedKey, sizeof(checkedKey), animationKey) ||
          sprintf_s(providerPrefix, sizeof(providerPrefix), "%s.",
             checkedProvider) < 0 ||
          strncmp(checkedKey, providerPrefix, strlen(providerPrefix)) != 0)
         return false;
   }

   unsigned char combatTargetDispatch = 4;
   if (!getExternalCombatTargetDispatch(spellId, kind,
                                        combatTargetDispatch))
      return false;

   return hasValidStandardExternalSpellJson(spellId, kind);
}

bool hasLoadableDefResource(char* const defName)
{
   _Def_* loadedDef = 0;
   __try
   {
      loadedDef = _Def_::Load(defName);
      if (!loadedDef)
         return false;
      loadedDef->DerefOrDestruct();
   }
   __except (EXCEPTION_EXECUTE_HANDLER)
   {
      return false;
   }
   return true;
}

bool initializeOwnedExternalAnimation(ExternalSpellSlot& slot,
                                      const int spellId)
{
   slot.hasCustomAnimation = false;
   slot.animationDef[0] = 0;
   slot.animationName[0] = 0;
   slot.animationKey[0] = 0;

   char* defName = 0;
   char* animationName = 0;
   char* animationKey = 0;
   int type = 0;
   const bool hasDef = tryGetJsonValue(makeExternalSpellJsonKey(
      spellId, "animationDef"), defName);
   const bool hasName = tryGetJsonValue(makeExternalSpellJsonKey(
      spellId, "animationName"), animationName);
   const bool hasType = tryGetJsonInt(makeExternalSpellJsonKey(
      spellId, "animationType"), type);
   const bool hasKey = tryGetJsonValue(makeExternalSpellJsonKey(
      spellId, "animationKey"), animationKey);

   if (!hasDef && !hasName && !hasType)
   {
      if (!hasKey)
         return true;
      return copyProviderKey(slot.animationKey, sizeof(slot.animationKey),
         animationKey);
   }

   char nameComponent[64] = {};
   if (!hasDef || !hasName || !hasType || type < 0 || type > 0xFFFF ||
       !copyProviderKey(slot.animationDef, sizeof(slot.animationDef), defName) ||
       !copyProviderKey(nameComponent, sizeof(nameComponent), animationName) ||
       sprintf_s(slot.animationName, sizeof(slot.animationName), "%s.%s.%s",
          slot.providerKey, slot.spellKey, nameComponent) < 0 ||
       strcpy_s(slot.animationKey, sizeof(slot.animationKey),
          slot.animationName) != 0 ||
       (hasKey && strcmp(animationKey, slot.animationKey) != 0))
      return false;

   slot.hasCustomAnimation = true;
   // Probe the final ERA resource tree while registration is sealing. LoadDef
   // holds a reference whether the DEF was already cached or was read from a
   // provider LOD; immediately release that reference after the existence
   // check. Missing or malformed resources fail this slot closed before its
   // animation-table entry becomes visible to the engine.
   return hasLoadableDefResource(slot.animationDef);
}

bool hasSyntacticallyValidExternalDeclaration(const int spellId)
{
   char* providerKey = 0;
   char* spellKey = 0;
   char* kind = 0;
   int capabilities = 0;
   return getExternalSpellDeclaration(spellId, providerKey, spellKey, kind,
      capabilities);
}

bool validateExternalSpellRegistration(ExternalSpellSlot& slot,
                                       const int spellId)
{
   char* providerKey = 0;
   char* spellKey = 0;
   char* kind = 0;
   int capabilities = 0;
   if (!slot.registered || slot.faulted || slot.contested ||
       !getExternalSpellDeclaration(spellId, providerKey, spellKey, kind,
                                    capabilities) ||
       strcmp(slot.providerKey, providerKey) != 0 ||
       strcmp(slot.spellKey, spellKey) != 0 ||
       slot.descriptor.spellId != spellId ||
       slot.descriptor.capabilities !=
          static_cast<std::uint32_t>(capabilities))
      return false;

   const std::uint32_t caps = slot.descriptor.capabilities;
   const std::uint32_t combatDomainCaps =
      NEWSPELLS_CAP_COMBAT_TARGET | NEWSPELLS_CAP_COMBAT_CAST |
      NEWSPELLS_CAP_STATUS_APPLY | NEWSPELLS_CAP_STATUS_ROUND |
      NEWSPELLS_CAP_STATUS_REMOVE | NEWSPELLS_CAP_CURE_DISPEL |
      NEWSPELLS_CAP_BATTLE_LIFECYCLE | NEWSPELLS_CAP_CREATURE_CAST |
      NEWSPELLS_CAP_ERM_CAST | NEWSPELLS_CAP_COMBAT_AI;
   if (!getExternalCombatTargetDispatch(spellId, kind,
                                        slot.combatTargetDispatch))
      return false;
   if (strcmp(kind, "adventure") == 0)
      return (caps & NEWSPELLS_CAP_ADVENTURE_CAST) &&
         !(caps & combatDomainCaps);
   if (strcmp(kind, "combat") == 0)
      return (caps & NEWSPELLS_CAP_COMBAT_CAST) &&
         !(caps & NEWSPELLS_CAP_ADVENTURE_CAST);
   if (strcmp(kind, "hybrid") == 0)
      return (caps & NEWSPELLS_CAP_ADVENTURE_CAST) &&
         (caps & NEWSPELLS_CAP_COMBAT_CAST);
   return false;
}

void sealAndValidateExternalSpellRegistrations()
{
   providerRegistrationOpen = false;
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
      const bool withinBoundary = spellId < activeSpellCount;
      const bool registrationValid = withinBoundary &&
         validateExternalSpellRegistration(*slot, spellId);
      const bool animationValid = registrationValid &&
         initializeOwnedExternalAnimation(*slot, spellId);
      slot->active = animationValid;
      spellIndirectTableA[spellId - SPELL_QUICKSAND] =
         slot->active ? static_cast<char>(slot->combatTargetDispatch) : 4;
      externalAnimationIndex[spellId - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID] = -1;

      if (!slot->active &&
          (slot->registered || hasSyntacticallyValidExternalDeclaration(spellId)))
      {
         char diagnostic[192] = {};
         const char* reason = !withinBoundary
            ? "outside the configured physical spell boundary"
            : (!slot->registered
               ? "no provider DLL registered this declared slot"
               : (!registrationValid
                  ? "provider descriptor and merged JSON did not match"
                  : "custom animation validation or resource loading failed"));
         sprintf_s(diagnostic, sizeof(diagnostic),
            "Spell ID %d disabled: %s.", spellId, reason);
         Era::WriteLog("NewSpells", "External spell activation", diagnostic);
      }
   }
}

void deactivateAllExternalSpellSlots()
{
   providerRegistrationOpen = false;
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
      slot->active = false;
      slot->faulted = true;
      spellIndirectTableA[spellId - SPELL_QUICKSAND] = 4;
      externalAnimationIndex[spellId - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID] = -1;
      if (pGame)
         pGame->DisableSpell(static_cast<SpellID>(spellId));
   }
}

bool isActiveExternalSpell(const int spellId)
{
   const ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
   return slot && slot->active && !slot->faulted;
}

bool getExternalAnimation(const int spellId, char*& defName, char*& name,
                          int& type)
{
   defName = name = 0;
   type = 0;
   ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
   if (!slot || !slot->active || !slot->hasCustomAnimation)
      return false;
   defName = slot->animationDef;
   name = slot->animationName;
   tryGetJsonInt(makeExternalSpellJsonKey(spellId, "animationType"), type);
   return true;
}

int countExternalAnimations()
{
   int count = 0;
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      if (!isActiveExternalSpell(spellId))
         continue;
      char* defName = 0;
      char* name = 0;
      int type = 0;
      if (getExternalAnimation(spellId, defName, name, type))
         ++count;
   }
   return count;
}

int getLogicalAnimationIndex(const char* animationKey)
{
   static const char* const animationKeys[] =
   {
      "Fear", "DeathCloud", "DeathBlow", "Toughness", "Claws",
      "Incineration", "HourOfPower", "GoldenTouch"
   };

   if (!animationKey || !*animationKey)
      return ID_NONE;

   for (std::size_t i = 0; i < sizeof(animationKeys) / sizeof(animationKeys[0]); ++i)
      if (strcmp(animationKey, animationKeys[i]) == 0)
         return customAnimationBase + static_cast<int>(i);

   return ID_NONE;
}

int getConfiguredSpellCount()
{
   const char* const jsonKey = "NewSpells.Config.MaxSpellId";
   int configuredMaxId = SPELL_GOLDEN_TOUCH;
   tryGetJsonInt(jsonKey, configuredMaxId);

   // Existing executable loops encode their exclusive bound as a signed imm8.
   // Count 128 would become -128, so 127 is the largest profile that can be
   // enabled without rewriting every compare/branch pair. The sidecars retain
   // 128 slots and WoG retains all 200 metadata records.
   const int maxActiveSpellId = NS_MAX_SPELL_ID;
   int highestDeclaredId = clampConfigInt(configuredMaxId,
      DEFAULT_SPELLS_NUM - 1, maxActiveSpellId);
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
      if (hasSyntacticallyValidExternalDeclaration(spellId))
         highestDeclaredId = max(highestDeclaredId, spellId);

   return highestDeclaredId + 1;
}

int getLiveSpellCount()
{
   return nsBoundHooksAttempted && activeSpellCount >= ORIG_SPELLS_NUM &&
      activeSpellCount <= WOG_SPELLS_MAX ? activeSpellCount : 0;
}

_Spell_* canonicalSpellTable = 0;

bool isValidSpellId(const int spellId)
{
   const int count = getLiveSpellCount();
   return count && spellId >= 0 && spellId < count;
}

bool isDefinedHeroSpell(const int spellId)
{
   if (!isValidSpellId(spellId) || !canonicalSpellTable ||
       o_Spell != canonicalSpellTable ||
       (spellId >= NEWSPELLS_EXTERNAL_SPELL_FIRST_ID &&
        !isActiveExternalSpell(spellId)))
      return false;

   const _Spell_& spell = canonicalSpellTable[spellId];
   return spell.name && *spell.name && spell.level >= 1 && spell.level <= 5 &&
      !(spell.flags & SF_CREATURE_SPELL);
}

void fillSpell(const int spellId)
{
   static char defaultSpellSound[] = "Bless.wav";
   _Spell_& spell = newSpellDefaults[spellId];
   memset(&spell, 0, sizeof(spell));

   spell.type = getJsonInt(makeSpellJsonKey(spellId, "type"), 1);
   spell.wav_name = getJsonString(makeSpellJsonKey(spellId, "soundName"),
      defaultSpellSound);

   int animationId = getJsonInt(makeSpellJsonKey(spellId, "animationIndex"), 0x24);
   if (animationId < 0 || animationId >= activeAnimationCount)
      animationId = 0x24;

   ExternalSpellSlot* const external = getExternalSpellSlot(spellId);
   if (external && external->active)
   {
      const int index = spellId - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
      if (externalAnimationIndex[index] >= 0)
         animationId = externalAnimationIndex[index];
      else if (external->animationKey[0])
      {
         bool foundAnimation = false;
         for (int i = 0; i < activeAnimationCount; ++i)
            if (SpellAnim[i].name &&
                strcmp(SpellAnim[i].name, external->animationKey) == 0)
            {
               animationId = i;
               foundAnimation = true;
               break;
            }
         if (!foundAnimation)
         {
            failExternalSpellClosed(*external);
            return;
         }
      }
   }
   else
   {
      char* animationKey = 0;
      if (tryGetJsonValue(makePrivateSpellJsonKey(spellId, "animationKey"), animationKey))
      {
         const int logicalAnimationId = getLogicalAnimationIndex(animationKey);
         if (logicalAnimationId >= 0 && logicalAnimationId < activeAnimationCount)
            animationId = logicalAnimationId;
      }
   }

   spell.animation_ix = animationId;
   spell.flags = getJsonInt(makeSpellJsonKey(spellId, "flags"), 0x40845);
   spell.name = getJsonString(makeSpellJsonKey(spellId, "name"), emptyLocalizedText);
   spell.short_name = getJsonString(makeSpellJsonKey(spellId, "shortName"),
      emptyLocalizedText);
   spell.level = getJsonInt(makeSpellJsonKey(spellId, "level"), 1);
   spell.school_flags = getJsonInt(makeSpellJsonKey(spellId, "school"), 0);

   static const int defaultManaCost[] = {5, 4, 4, 4};
   static const int defaultBaseValue[] = {0, 0, 1, 1};
   static const int defaultAiValue[] = {10, 10, 11, 11};
   for (int mastery = eMasteryNone; mastery <= eMasteryExpert; ++mastery)
   {
      spell.mana_cost[mastery] = getJsonInt(
         makeSpellJsonKey(spellId, "manaCost", mastery), defaultManaCost[mastery]);
      spell.effect[mastery] = getJsonInt(
         makeSpellJsonKey(spellId, "baseValue", mastery), defaultBaseValue[mastery]);
      spell.ai_value[mastery] = getJsonInt(
         makeSpellJsonKey(spellId, "aiValue", mastery), defaultAiValue[mastery]);
      spell.description[mastery] = getJsonString(
         makeSpellJsonKey(spellId, "description", mastery), emptyLocalizedText);
   }

   spell.eff_power = getJsonInt(makeSpellJsonKey(spellId, "spEffect"), 0);
   for (int town = CASTLE; town <= CONFLUX; ++town)
      spell.chance2get_var[town] = getJsonInt(
         makeSpellJsonKey(spellId, "chanceToGet", town), 10);

   // A provider JSON file with the same VFS-relative name as the core file
   // can hide the complete built-in catalog from ERA's translation loader.
   // Never turn fallback values into an apparently valid hero spell: ERM
   // clients would discover it through SS and then have HE:M reject its blank
   // identity.  Keep an incomplete record unmanaged (and therefore on WoG's
   // original definition) so every public path fails closed consistently.
   bool completeRecord = spell.wav_name && *spell.wav_name &&
      spell.name && *spell.name && spell.short_name && *spell.short_name &&
      spell.level >= 1 && spell.level <= 5 &&
      !(spell.flags & SF_CREATURE_SPELL) &&
      !(spell.flags & ~0x001FFFFFu) &&
      (spell.flags & (SF_BATTLE_SPELL | SF_MAP_SPELL)) &&
      !(spell.school_flags & ~0xFu) &&
      spell.animation_ix >= 0 && spell.animation_ix < activeAnimationCount;
   for (int mastery = eMasteryNone;
        completeRecord && mastery <= eMasteryExpert; ++mastery)
      completeRecord = spell.description[mastery] &&
         *spell.description[mastery] && spell.mana_cost[mastery] >= 0;

   if (!completeRecord)
   {
      char diagnostic[128];
      sprintf_s(diagnostic, sizeof(diagnostic),
         "Spell ID %d has incomplete era.spells metadata; its slot was left unmanaged.",
         spellId);
      Era::WriteLog("NewSpells", "Spell definition rejected", diagnostic);
      memset(&spell, 0, sizeof(spell));
      managedSpellDefaults[spellId] = false;
      if (external && external->active)
         failExternalSpellClosed(*external);
      return;
   }

   // Provider mechanics and duration policy stay provider-owned; the core's
   // private duration cap remains available only to its built-in spells.
   forceCappedDuration[spellId] = external && external->active ? 0 :
      clampConfigInt(getJsonInt(makePrivateSpellJsonKey(
         spellId, "maxDurationRounds"), 0), 0, 10000);

   if (spellId == SPELL_MOBILITY)
      mobilityRequiresBattle = getJsonInt(makePrivateSpellJsonKey(
         spellId, "requiresBattleBetweenCasts"), 0) != 0;
   if (spellId == SPELL_INCINERATION)
      incinerationSpecialFeature = getJsonInt(makePrivateSpellJsonKey(
         spellId, "permanentCasualties"), 0) != 0;
   if (spellId == SPELL_EXPLOSION)
      explosionSpeedReduction = getJsonInt(makePrivateSpellJsonKey(
         spellId, "temporarySpeedPenalty"), 0) != 0;

   managedSpellDefaults[spellId] = true;
}

_Spell_* const wogSpellBackup = reinterpret_cast<_Spell_*>(0x028AB210);
int (*const wogSpellZVars)[7] = reinterpret_cast<int (*)[7]>(0x028B1C50);

void restoreNewSpellDefaults(const bool updateWogBackup)
{
   if (!newSpellDefaultsReady || !canonicalSpellTable || o_Spell != canonicalSpellTable)
      return;

   for (int spellId = 0; spellId < activeSpellCount; ++spellId)
   {
      if (!managedSpellDefaults[spellId])
         continue;
      canonicalSpellTable[spellId] = newSpellDefaults[spellId];
      if (updateWogBackup)
         wogSpellBackup[spellId] = newSpellDefaults[spellId];
   }

   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      if (managedSpellDefaults[spellId])
         continue;
      memset(&canonicalSpellTable[spellId], 0, sizeof(_Spell_));
      if (updateWogBackup)
         memset(&wogSpellBackup[spellId], 0, sizeof(_Spell_));
   }
}

void restoreLegacySpellNumericFields()
{
   if (!newSpellDefaultsReady || !canonicalSpellTable || o_Spell != canonicalSpellTable)
      return;

   for (int spellId = 0; spellId < activeSpellCount; ++spellId)
   {
      if (!managedSpellDefaults[spellId])
         continue;
      _Spell_& live = canonicalSpellTable[spellId];
      const _Spell_& defaults = newSpellDefaults[spellId];

      char* const wavName = live.wav_name;
      char* const name = live.name;
      char* const shortName = live.short_name;
      char* descriptions[4] =
      {
         live.description[0], live.description[1],
         live.description[2], live.description[3]
      };

      live = defaults;
      live.wav_name = wavName;
      live.name = name;
      live.short_name = shortName;
      for (int mastery = 0; mastery < 4; ++mastery)
         live.description[mastery] = descriptions[mastery];

      // Legacy WoG saves can carry obsolete Z indices for the second table
      // relocation. Keep the already-replayed live pointers, but detach those
      // stale indices so native SN:H/SS owns subsequent replay and saving.
      memset(wogSpellZVars[spellId], 0, sizeof(wogSpellZVars[spellId]));
   }
}

const char NEW_SPELL_SAVE_SECTION[] = "NewSpells.SpellTable";
const std::uint32_t NEW_SPELL_SAVE_VERSION = 1;

void __stdcall saveSpellTableVersion(Era::TEvent* Event)
{
   if (!newSpellDefaultsReady || !canonicalSpellTable ||
       o_Spell != canonicalSpellTable)
      return;

   std::uint32_t version = NEW_SPELL_SAVE_VERSION;
   Era::WriteSavegameSection(sizeof(version), &version, NEW_SPELL_SAVE_SECTION);
}

void __stdcall loadSpellTableVersion(Era::TEvent* Event)
{
   std::uint32_t version = 0;
   const int bytesRead = Era::ReadSavegameSection(sizeof(version), &version,
      NEW_SPELL_SAVE_SECTION);

   if (bytesRead == 0)
      restoreLegacySpellNumericFields();
   else if (bytesRead != static_cast<int>(sizeof(version)) ||
            version != NEW_SPELL_SAVE_VERSION)
   {
      // A present but partial/future marker is not a legacy save. Leave WoG's
      // canonical load untouched and fail closed instead of guessing a format.
      Era::WriteLog("NewSpells", "Spell table save section",
         "Ignored an unsupported NewSpells.SpellTable save marker.");
   }
   // Version 1 is already restored by WoG's canonical 200-record loader.
}

int __stdcall restoreSpellDefaultsAfterWogReset(LoHook* h, HookContext* c)
{
   restoreNewSpellDefaults(true);
   return EXEC_DEFAULT;
}

char* getNewSpellDefaultText(HiHook* h, int spellId, int textType)
{
   char* result = CALL_2(char*, __cdecl, h->GetDefaultFunc(), spellId, textType);
   if (result || !newSpellDefaultsReady || textType < 0 || textType > 6 ||
       !isValidSpellId(spellId) || wogSpellZVars[spellId][textType] != 0)
      return result;

   if (!managedSpellDefaults[spellId])
      return result;

   _Spell_& defaults = newSpellDefaults[spellId];
   switch (textType)
   {
   case 0:
      return defaults.name;
   case 1:
      return defaults.short_name;
   case 2:
   case 3:
   case 4:
   case 5:
      return defaults.description[textType - 2];
   case 6:
      return defaults.wav_name;
   default:
      return result;
   }
}

int __stdcall ermHeroSpellQuery(LoHook* h, HookContext* c)
{
   hero* const Hero = *reinterpret_cast<hero**>(c->ebp - 0x380);
   const int spellId = *reinterpret_cast<int*>(c->ebp - 0x54);

   if (!Hero || !isDefinedHeroSpell(spellId))
   {
      c->return_address = 0x749645;
      return NO_EXEC_DEFAULT;
   }

   *reinterpret_cast<unsigned char*>(0x91F2E0) = heroInSpellbook(Hero, spellId) != 0;
   c->return_address = 0x7459F6;
   return NO_EXEC_DEFAULT;
}

int __stdcall ermHeroSpellSet(LoHook* h, HookContext* c)
{
   hero* const Hero = *reinterpret_cast<hero**>(c->ebp - 0x380);
   const int spellId = *reinterpret_cast<int*>(c->ebp - 0x54);
   void* const message = reinterpret_cast<void*>(c->ebp - 0x300);

   if (!Hero || !isDefinedHeroSpell(spellId))
   {
      c->return_address = 0x749645;
      return NO_EXEC_DEFAULT;
   }

   const int applyResult = CALL_4(int, __cdecl, 0x74195D,
      &heroAvailableSpell(Hero, spellId), 1, message, 1);
   if (applyResult)
   {
      c->return_address = 0x74943B;
      return NO_EXEC_DEFAULT;
   }

   heroInSpellbook(Hero, spellId) = heroAvailableSpell(Hero, spellId) != 0;
   c->return_address = 0x7459F6;
   return NO_EXEC_DEFAULT;
}

int __stdcall ermSpellDisabled(LoHook* h, HookContext* c)
{
   static_assert(0x4A - ORIG_SPELLS_NUM == 4,
      "UN:J0 must address the unified Game+4 disabled-spell array");

   int& spellId = *reinterpret_cast<int*>(c->ebp - 0x44);
   if (!isValidSpellId(spellId))
   {
      c->return_address = 0x733833;
      return NO_EXEC_DEFAULT;
   }

   if (spellId >= NS_DISABLED_SLOTS)
   {
      int value = nsDisabledEx[spellId - NS_DISABLED_SLOTS];
      void* const message = *reinterpret_cast<void**>(c->ebp + 0x14);
      if (CALL_4(int, __cdecl, 0x74195D, &value, 4, message, 2))
      {
         c->return_address = 0x733F2B;
         return NO_EXEC_DEFAULT;
      }
      nsDisabledEx[spellId - NS_DISABLED_SLOTS] = (unsigned char)value;
      c->return_address = 0x733F2F;
      return NO_EXEC_DEFAULT;
   }

   // Stock SpellDisBase returns Game+0x4A. Translating every ID by -70 makes
   // the untouched receiver operate on New Spells' unified Game+4 array.
   spellId -= ORIG_SPELLS_NUM;
   c->return_address = 0x733853;
   return NO_EXEC_DEFAULT;
}

void* resolveLegacyErmBattleField(army* const Army, const int index,
   const std::uint32_t offset)
{
   static_assert(sizeof(void*) == 4, "BM:G requires the x86 ERM ABI");
   static_assert(sizeof(army) == NewSpellsErm::StackSize &&
      sizeof(_BattleStack_) == NewSpellsErm::StackSize,
      "BM:G battle stack stride mismatch");
   static_assert(offsetof(army, spellInfluence) == NewSpellsErm::DurationOffset &&
      offsetof(army, SpellInfluenceQueue) ==
         NewSpellsErm::MasteryOffset + NewSpellsErm::OriginalSpellCount * 4u,
      "BM:G original spell field layout mismatch");
   static_assert(sizeof(pCombatManager->stack) ==
      NewSpellsErm::StackCount * NewSpellsErm::StackSize,
      "BM:G battle stack array mismatch");

   const std::uint32_t address = NewSpellsErm::LegacyAddress(
      reinterpret_cast<std::uint32_t>(Army), index, offset);
   std::uint32_t slot, spell;
   if (pCombatManager && NewSpellsErm::OriginalMasterySlot(address,
       reinterpret_cast<std::uint32_t>(&pCombatManager->stack[0][0]),
       slot, spell))
      return &activeSpellMastery[slot / 21][slot % 21][spell];
   return reinterpret_cast<void*>(address);
}

int __stdcall ermBattleSpellInfluence(LoHook* h, HookContext* c)
{
   const int spellId = *reinterpret_cast<int*>(c->ebp - 0x24);
   army* const Army = *reinterpret_cast<army**>(c->ebp - 0x10);
   void* const message = *reinterpret_cast<void**>(c->ebp + 0x14);

   // The original receiver has already checked syntax and selected stack
   // 0..41. Legacy field access must not depend on mutable group/index fields
   // or the configured spell count. Added spell IDs always keep their meaning.
   if (spellId < SPELL_FEAR || spellId > NEWSPELLS_EXTERNAL_SPELL_LAST_ID)
   {
      if (spellId >= 0 && spellId < SPELL_FEAR)
      {
         int& duration = nsDuration(Army, spellId);
         int& mastery = *static_cast<int*>(resolveLegacyErmBattleField(
            Army, spellId, NewSpellsErm::MasteryOffset));
         const int oldDuration = duration;
         const int oldMastery = mastery;
         const int durationApply = CALL_4(int, __cdecl, 0x74195D,
            &duration, 4, message, 1);
         const int masteryApply = CALL_4(int, __cdecl, 0x74195D,
            &mastery, 4, message, 2);

         // Keep New Spells' removal behavior, but accept the original ERM
         // value domain and do not roll back writes or add an Unknown error
         // when Apply itself reports a malformed operand.
         if (durationApply >= 0 && masteryApply >= 0 &&
             oldDuration != 0 && duration == 0)
         {
            duration = oldDuration;
            mastery = oldMastery;
            Army->CancelIndividualSpell(spellId);
            mastery = eMasteryNone;
         }
      }
      else
      {
         // No pre-read, range clamp, dummy operand, or rollback: /d must keep
         // the ERA Apply routine's unused-operand behavior even for raw aliases.
         CALL_4(int, __cdecl, 0x74195D, resolveLegacyErmBattleField(
            Army, spellId, NewSpellsErm::DurationOffset), 4, message, 1);
         CALL_4(int, __cdecl, 0x74195D, resolveLegacyErmBattleField(
            Army, spellId, NewSpellsErm::MasteryOffset), 4, message, 2);
      }
      c->return_address = 0x75F370;
      return NO_EXEC_DEFAULT;
   }

   if (!message || !isRealArmy(Army) || !hasValidArmyCoordinates(Army) ||
       !isValidSpellId(spellId))
   {
      c->eax = 0;
      c->return_address = 0x75F867;
      return NO_EXEC_DEFAULT;
   }

   ExternalSpellSlot* const external = getExternalSpellSlot(spellId);
   if (external && (!external->active ||
       !(external->descriptor.capabilities & NEWSPELLS_CAP_ERM_CAST)))
   {
      c->eax = 0;
      c->return_address = 0x75F867;
      return NO_EXEC_DEFAULT;
   }

   int& duration = nsDuration(Army, spellId);
   int& mastery = activeSpellMastery[Army->group][Army->index][spellId];
   const int oldDuration = duration;
   const int oldMastery = mastery;

   const int durationApply = CALL_4(int, __cdecl, 0x74195D,
      &duration, 4, message, 1);
   const int masteryApply = CALL_4(int, __cdecl, 0x74195D,
      &mastery, 4, message, 2);

   // ZvsApply returns 1 for a valid query/comparison and -1 for an error.
   if (durationApply < 0 || masteryApply < 0 || duration < 0 ||
       mastery < eMasteryNone || mastery > eMasteryExpert)
   {
      duration = oldDuration;
      mastery = oldMastery;
      c->eax = 0;
      c->return_address = 0x75F867;
      return NO_EXEC_DEFAULT;
   }

   if (!external && oldDuration != 0 && duration == 0)
   {
      duration = oldDuration;
      mastery = oldMastery;
      Army->CancelIndividualSpell(spellId);
      mastery = eMasteryNone;
   }

   if (external && (oldDuration != duration || oldMastery != mastery))
   {
      NewSpellsCombatContextV1 context = {};
      context.size = sizeof(context);
      context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
      context.combatManager = pCombatManager;
      context.targetStack = Army;
      context.spellId = spellId;
      context.casterSide = ID_NONE;
      context.targetHex = Army->gridIndex;
      context.mastery = mastery;
      context.spellPower = duration;
      context.source = NEWSPELLS_SOURCE_ERM;
      const int32_t result = invokeCombatCallback(*external,
         external->descriptor.OnErmCast, context);
      if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
      {
         failExternalSpellClosed(*external);
         c->eax = 0;
         c->return_address = 0x75F867;
         return NO_EXEC_DEFAULT;
      }
      if (result != NEWSPELLS_PROVIDER_COMMITTED)
      {
         duration = oldDuration;
         mastery = oldMastery;
         c->eax = 0;
         c->return_address = 0x75F867;
         return NO_EXEC_DEFAULT;
      }

      if (oldDuration != 0 && duration == 0)
      {
         duration = oldDuration;
         mastery = oldMastery;
         Army->CancelIndividualSpell(spellId);
         mastery = eMasteryNone;
      }
   }

   c->return_address = 0x75F370;
   return NO_EXEC_DEFAULT;
}

bool hasValidExternalSpellRecord(const int spellId, const bool requireEnabled)
{
   if (!isValidSpellId(spellId) || !canonicalSpellTable ||
       o_Spell != canonicalSpellTable)
      return false;

   const _Spell_& spell = canonicalSpellTable[spellId];
   if (!spell.name || spell.level < 1 || spell.level > 5 ||
       !(spell.flags & SF_BATTLE_SPELL) || (spell.flags & SF_MAP_SPELL))
      return false;

   return !requireEnabled ||
      (pGame && !pGame->SpellDisabled(static_cast<SpellID>(spellId)));
}

bool isValidCreatureExperienceSpell(const int spellId,
                                    const bool requireEnabled = true)
{
   if (!hasValidExternalSpellRecord(spellId, requireEnabled))
      return false;

   const ExternalSpellSlot* const external = getExternalSpellSlot(spellId);
   if (external && external->active)
      return (external->descriptor.capabilities & NEWSPELLS_CAP_CREATURE_CAST) != 0;

   const _Spell_& spell = canonicalSpellTable[spellId];
   switch (spellId)
   {
   case SPELL_FEAR:
      return (spell.flags & (SF_TIME_SCALE | SF_SINGLE_TARGET | SF_MIND_SPELL)) ==
         (SF_TIME_SCALE | SF_SINGLE_TARGET | SF_MIND_SPELL);
   case SPELL_EXPLOSION:
      return (spell.flags & (SF_DAMAGE_SPELL | SF_SINGLE_TARGET)) ==
         (SF_DAMAGE_SPELL | SF_SINGLE_TARGET);
   case SPELL_GOLDEN_TOUCH:
      return (spell.flags & SF_DAMAGE_SPELL) != 0;
   default:
      return false;
   }
}

bool isValidOrdinaryCreatureExperienceSpell(const int spellId,
                                            const bool requireEnabled = true)
{
   if (!hasValidExternalSpellRecord(spellId, requireEnabled))
      return false;

   const ExternalSpellSlot* const external = getExternalSpellSlot(spellId);
   if (external && external->active)
      return (external->descriptor.capabilities & NEWSPELLS_CAP_CREATURE_CAST) != 0;

   switch (spellId)
   {
   case SPELL_SUMMON_FIREBIRD:
   case SPELL_TOUGHNESS:
   case SPELL_BEHEMOTHS_CLAWS:
   case SPELL_INCINERATION:
   case SPELL_EXPLOSION:
   case SPELL_HOUR_OF_POWER:
   case SPELL_GOLDEN_TOUCH:
      return true;
   default:
      return false;
   }
}

bool isValidStatusCreatureExperienceSpell(const int spellId,
                                          const bool requireEnabled = true)
{
   if (!hasValidExternalSpellRecord(spellId, requireEnabled))
      return false;

   const ExternalSpellSlot* const external = getExternalSpellSlot(spellId);
   if (external && external->active)
      return (external->descriptor.capabilities &
         (NEWSPELLS_CAP_CREATURE_CAST | NEWSPELLS_CAP_STATUS_APPLY)) ==
         (NEWSPELLS_CAP_CREATURE_CAST | NEWSPELLS_CAP_STATUS_APPLY);

   if (spellId != SPELL_TOUGHNESS && spellId != SPELL_BEHEMOTHS_CLAWS &&
       spellId != SPELL_HOUR_OF_POWER)
      return false;

   const _Spell_& spell = canonicalSpellTable[spellId];
   const unsigned int required = SF_TIME_SCALE | SF_SINGLE_TARGET;
   const unsigned int forbidden =
      SF_DAMAGE_SPELL | SF_TARGET_ANYWHERE | SF_MAP_SPELL;
   return (spell.flags & required) == required && !(spell.flags & forbidden);
}

int __stdcall ermCreatureAbilitySpellBound(LoHook* h, HookContext* c)
{
   const int spellId = *reinterpret_cast<int*>(c->ebp - 8);
   if (isValidCreatureExperienceSpell(spellId))
   {
      c->return_address = 0x71D8B0;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall ermCreatureAbilitySelectSpell(LoHook* h, HookContext* c)
{
   const int spellId = *reinterpret_cast<int*>(c->ebp - 8);
   if (spellId != SPELL_FEAR && spellId != SPELL_EXPLOSION &&
       spellId != SPELL_GOLDEN_TOUCH && !isActiveExternalSpell(spellId))
      return EXEC_DEFAULT;

   army* const caster = *reinterpret_cast<army**>(c->ebp + 8);
   army* const target = *reinterpret_cast<army**>(c->ebp + 0x0C);
   bool canCast = isValidCreatureExperienceSpell(spellId) &&
      isRealArmy(caster) && isRealArmy(target) &&
      hasValidArmyCoordinates(caster) && hasValidArmyCoordinates(target) &&
      pCombatManager;

   if (canCast)
   {
      int side = caster->group;
      if (caster->spellInfluence[SPELL_HYPNOTIZE])
         side = !side;

      canCast = CALL_6(bool, __thiscall, 0x5A8950, pCombatManager,
         spellId, side, target, 1, 1);
   }

   c->eax = canCast ? spellId : ID_NONE;
   c->return_address = 0x71D91D;
   return NO_EXEC_DEFAULT;
}

int __stdcall ermOrdinaryCreatureSpellBound(LoHook* h, HookContext* c)
{
   const int spellId = *reinterpret_cast<int*>(c->ebp - 8);
   if (isValidOrdinaryCreatureExperienceSpell(spellId))
   {
      c->return_address = 0x71DA69;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall ermStatusCreatureSpellBound(LoHook* h, HookContext* c)
{
   const int spellId = *reinterpret_cast<int*>(c->ebp - 0x0C);
   if (isValidStatusCreatureExperienceSpell(spellId))
   {
      c->return_address = 0x71DD3D;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall ermCreatureSpellInfoBound(LoHook* h, HookContext* c)
{
   const int spellId = *reinterpret_cast<int*>(c->ebp - 0x38);
   std::uintptr_t passAddress = 0;
   std::uintptr_t failAddress = 0;
   bool displayable = false;

   switch (h->GetAddress())
   {
   case 0x721D05:
      passAddress = 0x721D0B;
      failAddress = 0x721DCE;
       displayable = spellId < SPELL_FEAR ||
         isValidCreatureExperienceSpell(spellId, false);
      break;
   case 0x721EF0:
      passAddress = 0x721EF6;
      failAddress = 0x721FB9;
       displayable = spellId < SPELL_FEAR ||
         isValidOrdinaryCreatureExperienceSpell(spellId, false);
      break;
   case 0x722100:
      passAddress = 0x722106;
      failAddress = 0x7221CB;
       displayable = spellId < SPELL_FEAR ||
         isValidOrdinaryCreatureExperienceSpell(spellId, false);
      break;
   case 0x7222EF:
      passAddress = 0x7222F5;
      failAddress = 0x7223B8;
       displayable = spellId < SPELL_FEAR ||
         isValidOrdinaryCreatureExperienceSpell(spellId, false);
      break;
   case 0x7224F8:
      passAddress = 0x7224FE;
      failAddress = 0x7225C3;
       displayable = spellId < SPELL_FEAR ||
         isValidOrdinaryCreatureExperienceSpell(spellId, false);
      break;
   case 0x7227CC:
      passAddress = 0x7227D2;
      failAddress = 0x7228EC;
       displayable = spellId < SPELL_FEAR ||
         isValidStatusCreatureExperienceSpell(spellId, false);
      break;
   default:
      return EXEC_DEFAULT;
   }

   c->return_address = spellId >= 0 && displayable
      ? passAddress : failAddress;
   return NO_EXEC_DEFAULT;
}

char* getBoundedWogSpellName(HiHook* h, int spellId)
{
   // Keep every legacy lookup organic even before New Spells finishes its
   // post-initialization table population. WoG's canonical table safely owns
   // these records already.
   if (spellId >= 0 && spellId < SPELL_FEAR)
   {
      char* const legacyResult = CALL_1(char*, __cdecl,
         h->GetDefaultFunc(), spellId);
      return legacyResult ? legacyResult : emptyLocalizedText;
   }

   if (!isValidSpellId(spellId) || !canonicalSpellTable ||
       o_Spell != canonicalSpellTable || !canonicalSpellTable[spellId].name)
      return emptyLocalizedText;

   char* const result = CALL_1(char*, __cdecl, h->GetDefaultFunc(), spellId);
   return result ? result : emptyLocalizedText;
}

template<std::size_t N>
bool matchesCode(const std::uintptr_t address, const unsigned char (&expected)[N])
{
   return memcmp(reinterpret_cast<const void*>(address), expected, N) == 0;
}

bool hasCompatibleEraApplyHook()
{
   const unsigned char originalApply[] = {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C};
   if (matchesCode(0x74195D, originalApply))
      return true;

   Patch* const patch = _P->GetLastPatchAt(0x74195D);
   if (!patch || !patch->IsApplied() || patch->GetType() != PATCH_ ||
       patch->GetAddress() != 0x74195D || patch->GetSize() != 5 ||
       *reinterpret_cast<const unsigned char*>(0x74195D) != 0xE9)
      return false;

   const char* const owner = patch->GetOwner();
   if (!owner)
      return false;

   const char* fileName = strrchr(owner, '\\');
   if (!fileName)
      fileName = strrchr(owner, '/');
   fileName = fileName ? fileName + 1 : owner;
   return _stricmp(fileName, "era.dll") == 0;
}

bool canBindCanonicalWogSpellTable()
{
   unsigned char* const originalSpellTable = reinterpret_cast<unsigned char*>(0x6854A0);
   void* const registeredSpellTable = Era::GetRealAddr(originalSpellTable);
   _Spell_* const liveSpellTable = o_Spell;

   if (!liveSpellTable || reinterpret_cast<void*>(liveSpellTable) == originalSpellTable ||
       (registeredSpellTable != originalSpellTable &&
        registeredSpellTable != liveSpellTable))
   {
      char diagnostic[192];
      sprintf_s(diagnostic, sizeof(diagnostic),
         "original=%p, ERA registered=%p, 0x687FA8/o_Spell=%p",
         originalSpellTable, registeredSpellTable, liveSpellTable);
      Era::WriteLog("NewSpells", "WoG spell-table identity", diagnostic);
      return false;
   }

   const bool alreadyRegistered = registeredSpellTable == liveSpellTable;
   unsigned char* const liveBytes = reinterpret_cast<unsigned char*>(liveSpellTable);
   const std::size_t originalTableSize = ORIG_SPELLS_NUM * sizeof(_Spell_);
   for (std::size_t offset = 0; offset < originalTableSize; ++offset)
   {
      void* const expected = alreadyRegistered
         ? static_cast<void*>(liveBytes + offset)
         : static_cast<void*>(originalSpellTable + offset);
      if (Era::GetRealAddr(originalSpellTable + offset) != expected)
      {
         char diagnostic[192];
         sprintf_s(diagnostic, sizeof(diagnostic),
            "overlapping or non-linear ERA mapping at original offset 0x%X",
            static_cast<unsigned int>(offset));
         Era::WriteLog("NewSpells", "WoG spell-table identity", diagnostic);
         return false;
      }
   }

   return true;
}

bool bindCanonicalWogSpellTable()
{
   void* const originalSpellTable = reinterpret_cast<void*>(0x6854A0);
   _Spell_* const liveSpellTable = o_Spell;
   void* registeredSpellTable = Era::GetRealAddr(originalSpellTable);

   if (!canBindCanonicalWogSpellTable())
      return false;

   // Current WoG exposes its 200-record table through 0x687FA8 but does not
   // register the original SoD base in ERA's relocation registry. Register an
   // alias to WoG's existing block; no second live spell table is allocated.
   if (registeredSpellTable == originalSpellTable)
   {
      Era::RedirectMemoryBlock(originalSpellTable,
         ORIG_SPELLS_NUM * sizeof(_Spell_), liveSpellTable);
      registeredSpellTable = Era::GetRealAddr(originalSpellTable);
   }

   if (registeredSpellTable != liveSpellTable || o_Spell != liveSpellTable)
      return false;

   canonicalSpellTable = liveSpellTable;
   return true;
}

bool validateNativeErmProfile()
{
   const unsigned char countPrefix[] =
      {0x33,0xFF,0x57,0x8B,0xCE,0xE8,0xA1,0x6C,0x0D,0x00,0x47,0x83,0xFF};
   const unsigned char countSuffix[] = {0x7C,0xF2};
   const unsigned char heroQuery[] =
      {0x8B,0x85,0x80,0xFC,0xFF,0xFF,0x03,0x45,0xAC,0x0F,0xB6,0x88,0xEA,0x03,0x00,0x00,
       0x85,0xC9,0x75,0x09,0xC6,0x85,0x23,0xFC,0xFF,0xFF,0x00,0xEB,0x07,0xC6,0x85,0x23,
       0xFC,0xFF,0xFF,0x01,0x8A,0x95,0x23,0xFC,0xFF,0xFF,0x88,0x15,0xE0,0xF2,0x91,0x00,
       0xEB,0x74};
   const unsigned char heroSetter[] =
      {0x83,0x7D,0xAC,0x00,0x7C,0x06,0x83,0x7D,0xAC,0x45,0x7E,0x05,0xE9,0xB2,0x3C,0x00,0x00};
   const unsigned char heroSetterBody[] =
      {0x6A,0x01,0x8D,0x85,0x00,0xFD,0xFF,0xFF,0x50,0x6A,0x01,0x8B,0x4D,0xAC,0x8B,0x95,
       0x80,0xFC,0xFF,0xFF,0x8D,0x84,0x0A,0x30,0x04,0x00,0x00,0x50,0xE8,0xA9,0xBF,0xFF,
       0xFF,0x83,0xC4,0x10,0x85,0xC0,0x74,0x05,0xE9,0x7B,0x3A,0x00,0x00,0x8B,0x8D,0x80,
       0xFC,0xFF,0xFF,0x03,0x4D,0xAC,0x0F,0xB6,0x91,0x30,0x04,0x00,0x00,0x85,0xD2,0x75,
       0x12,0x8B,0x85,0x80,0xFC,0xFF,0xFF,0x03,0x45,0xAC,0xC6,0x80,0xEA,0x03,0x00,0x00,
       0x00,0xEB,0x10,0x8B,0x8D,0x80,0xFC,0xFF,0xFF,0x03,0x4D,0xAC,0xC6,0x81,0xEA,0x03,
       0x00,0x00,0x01,0xE9,0x05,0x15,0x00,0x00};
   const unsigned char heroSuccess[] = {0xE9,0x05,0x15,0x00,0x00};
   const unsigned char heroReceiverExit[] = {0xE9,0x36,0x25,0x00,0x00};
   const unsigned char heroApplyFailure[] =
      {0x8B,0x85,0x04,0xFD,0xFF,0xFF,0x03,0x85,0x00,0xFD,0xFF,0xFF,0x0F,0xBE,0x08,
       0x83,0xF9,0x3B,0x0F,0x85,0x05,0x8A,0xFF,0xFF};
   const unsigned char heroInvalid[] =
      {0xE9,0x8F,0x00,0x00,0x00,0xE9,0x8A,0x00,0x00,0x00};
   const unsigned char spellDisabled[] =
      {0x83,0x7D,0xBC,0x00,0x7C,0x06,0x83,0x7D,0xBC,0x46,0x7C,0x20};
   const unsigned char spellDisabledError[] =
      {0x68,0x0C,0x62,0x79,0x00,0x68,0xE8,0x08,0x00,0x00,0x6A,0x01,0xE8,0xEF,0xEA,0xFD,
       0xFF,0x83,0xC4,0x0C,0x90,0x90,0x90,0x90,0x90,0x33,0xC0,0xE9,0x04,0x07,0x00,0x00};
   const unsigned char spellDisabledBody[] =
      {0xE8,0x81,0x2A,0xFE,0xFF,0x8B,0x4D,0xBC,0x0F,0xBE,0x14,0x08,0x89,0x55,0xF4,0x6A,
       0x02,0x8B,0x45,0x14,0x50,0x6A,0x04,0x8D,0x4D,0xF4,0x51,0xE8,0xEA,0xE0,0x00,0x00,
       0x83,0xC4,0x10,0x85,0xC0,0x74,0x05,0xE9,0xAF,0x06,0x00,0x00,0xE8,0x55,0x2A,0xFE,
       0xFF,0x8B,0x55,0xBC,0x8A,0x4D,0xF4,0x88,0x0C,0x10,0xE9,0x9C,0x06,0x00,0x00};
   const unsigned char battleInfluence[] =
      {0x6A,0x01,0x8B,0x4D,0x14,0x51,0x6A,0x04,0x8B,0x55,0xDC,0x8B,0x45,0xF0,0x8D,0x8C,
       0x90,0x98,0x01,0x00,0x00,0x51,0xE8,0x0E,0x26,0xFE,0xFF,0x83,0xC4,0x10,0x6A,0x02,
       0x8B,0x55,0x14,0x52,0x6A,0x04,0x8B,0x45,0xDC,0x8B,0x4D,0xF0,0x8D,0x94,0x81,0xDC,
       0x02,0x00,0x00,0x52,0xE8,0xF0,0x25,0xFE,0xFF,0x83,0xC4,0x10,0xE9,0xE8,0x04,0x00,0x00};
   const unsigned char resetSeam[] =
      {0x68,0xE0,0x15,0x00,0x00,0x6A,0x00,0x68,0x50,0x1C,0x8B,0x02,0xE8,0xB7,0x77,0x00,
       0x00,0x83,0xC4,0x0C,0x90,0x90,0x90,0x90,0x90,0x5F,0x5E,0x5B,0x5D,0xC3};
   const unsigned char backupRestore[] =
      {0x8B,0x75,0xFC,0x69,0xF6,0x88,0x00,0x00,0x00,0x81,0xC6,0x10,0xB2,0x8A,0x02,
       0x8B,0x7D,0xFC,0x69,0xFF,0x88,0x00,0x00,0x00,0x81,0xC7,0xC0,0xD2,0x7B,0x00,
       0xB9,0x22,0x00,0x00,0x00,0xF3,0xA5};
   const unsigned char backupCapture[] =
      {0x8B,0x75,0xFC,0x69,0xF6,0x88,0x00,0x00,0x00,0x81,0xC6,0xC0,0xD2,0x7B,0x00,
       0x8B,0x7D,0xFC,0x69,0xFF,0x88,0x00,0x00,0x00,0x81,0xC7,0x10,0xB2,0x8A,0x02,
       0xB9,0x22,0x00,0x00,0x00,0xF3,0xA5};
   const unsigned char getTextEntry[] =
      {0x55,0x8B,0xEC,0x83,0xEC,0x08,0x53,0x56,0x57};
   const unsigned char getTextBody[] =
      {0xC7,0x45,0xF8,0x00,0x00,0x00,0x00,0x8B,0x45,0x08,0x6B,0xC0,0x1C,0x8B,0x4D,0x0C,
       0x8B,0x94,0x88,0x50,0x1C,0x8B,0x02,0x89,0x55,0xFC,0x83,0x7D,0xFC,0x00,0x74,0x32,
       0x81,0x7D,0xFC,0xE8,0x03,0x00,0x00,0x7E,0x11,0x8B,0x45,0xFC,0x50,0xE8,0xD0,0x0E,
       0x00,0x00,0x83,0xC4,0x04,0x89,0x45,0xF8,0xEB,0x18,0x83,0x7D,0xFC,0x00,0x7E,0x12,
       0x8B,0x4D,0xFC,0x83,0xE9,0x01,0xC1,0xE1,0x09,0x81,0xC1,0xE8,0x73,0x92,0x00,0x89,
       0x4D,0xF8,0x90,0x90,0x90,0x90,0x90,0x8B,0x45,0xF8,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,
       0xC3};
   const unsigned char battleInfluenceSuccess[] = {0xE9,0xE8,0x04,0x00,0x00};
   const unsigned char battleInfluenceEpilogue[] =
      {0x90,0x90,0x90,0x90,0x90,0xB8,0x01,0x00,0x00,0x00,0x5F,0x5E,0x5B,0x8B,0xE5,0x5D,
       0xC3};
   const unsigned char creatureAbilityBound[] =
      {0x83,0x7D,0xF8,0x57,0x7C,0x0A,0x90,0x90,0x90,0x90,0x90,0x83,0xC8,0xFF,0xEB,0x7B};
   const unsigned char creatureAbilitySelect[] =
      {0xE8,0xAD,0xDB,0x03,0x00,0x83,0xC4,0x18,0x89,0x45,0xF8,0x90,0x90,0x90,0x90,0x90};
   const unsigned char creatureCastSite[] =
      {0x8B,0x96,0x4C,0xFF,0xFF,0xFF,0x8B,0x4D,0xF8,0x6A,0x03,0x6A,0x00,0x6A,0xFF,0x6A,
       0x01,0x52,0x50,0xE8,0x59,0x74,0x13,0x00,0xC7,0x06,0xFF,0xFF,0xFF,0xFF};
   const unsigned char ordinaryCreatureBound[] =
      {0x83,0x7D,0xF8,0x57,0x7C,0x05,0xE9,0x78,0xFF,0xFF,0xFF,0x6A,0x64,0x6A,0x01,0xE8,
       0x97,0x2A,0xFF,0xFF,0x83,0xC4,0x08};
   const unsigned char statusCreatureBound[] =
      {0x83,0x7D,0xF4,0x57,0x7C,0x0A,0x90,0x90,0x90,0x90,0x90,0xE9,0xC2,0x06,0x00,0x00,
       0x83,0x7D,0xFC,0x04,0x7D,0x20};
   const unsigned char infoCreatureC[] =
      {0x83,0x7D,0xC8,0x57,0x0F,0x8D,0xC3,0x00,0x00,0x00,0x68,0x88,0x7D,0x84,0x00,0x6A,
       0x01,0x68,0x84,0x00,0x00,0x00};
   const unsigned char infoCreatureP[] =
      {0x83,0x7D,0xC8,0x57,0x0F,0x8D,0xC3,0x00,0x00,0x00,0x68,0x88,0x7D,0x84,0x00,0x6A,
       0x01,0x68,0x8E,0x00,0x00,0x00};
   const unsigned char infoCreatureJ[] =
      {0x83,0x7D,0xC8,0x57,0x0F,0x8D,0xC5,0x00,0x00,0x00,0x68,0x88,0x7D,0x84,0x00,0x6A,
       0x01,0x8B,0x4D,0xF0,0x83,0xC1,0x02,0x51,0xE8,0xF2,0x4F,0x05,0x00};
   const unsigned char infoCreatureA[] =
      {0x83,0x7D,0xC8,0x57,0x0F,0x8D,0xC3,0x00,0x00,0x00,0x68,0x88,0x7D,0x84,0x00,0x6A,
       0x01,0x68,0x94,0x00,0x00,0x00};
   const unsigned char infoCreatureK[] =
      {0x83,0x7D,0xC8,0x57,0x0F,0x8D,0xC5,0x00,0x00,0x00,0x68,0x88,0x7D,0x84,0x00,0x6A,
       0x01,0x8B,0x4D,0xF0,0x83,0xC1,0x02,0x51,0xE8,0xFA,0x4B,0x05,0x00};
   const unsigned char infoCreatureS[] =
      {0x83,0x7D,0xC8,0x57,0x0F,0x8D,0x1A,0x01,0x00,0x00,0x8B,0x4D,0xD0,0x03,0x4D,0xFC,
       0x0F,0xB6,0x51,0x06,0x83,0xFA,0x04};
   const unsigned char getSpellNameBody[] =
      {0xBA,0xA8,0x7F,0x68,0x00,0x8B,0x12,0x8B,0x45,0x08,0x8B,0xC8,0xC1,0xE1,0x04,0x03,
       0xC8,0x8B,0x44,0xCA,0x10,0x89,0x45,0xFC};
   const unsigned char getSpellNameEntry[] =
      {0x55,0x8B,0xEC,0x51,0x53,0x56,0x57};

   const bool getTextCompatible = !_P->GetLastPatchAt(0x775702) &&
      matchesCode(0x775702, getTextEntry) && matchesCode(0x77571E, getTextBody);
   const bool getNameCompatible = !_P->GetLastPatchAt(0x7149AF) &&
      matchesCode(0x7149AF, getSpellNameEntry);

   bool valid = true;
#define CHECK_NATIVE_PROFILE(label, expression) \
   do { if (!(expression)) { Era::WriteLog("NewSpells", "Native ERM profile mismatch", label); valid = false; } } while (0)

   CHECK_NATIVE_PROFILE("WoG canonical spell table binding", canBindCanonicalWogSpellTable());
   CHECK_NATIVE_PROFILE("spell-count prefix at 0x4028F5", matchesCode(0x4028F5, countPrefix));
   CHECK_NATIVE_PROFILE("spell-count suffix at 0x402903", matchesCode(0x402903, countSuffix));
   CHECK_NATIVE_PROFILE("stock spell count at 0x402902", *reinterpret_cast<const unsigned char*>(0x402902) == ORIG_SPELLS_NUM);
   CHECK_NATIVE_PROFILE("WoG metadata capacity at 0x7751F0", *reinterpret_cast<const std::uint32_t*>(0x7751F0) == WOG_SPELLS_MAX);
   CHECK_NATIVE_PROFILE("HE:M query at 0x745950", matchesCode(0x745950, heroQuery));
   CHECK_NATIVE_PROFILE("HE:M setter at 0x745982", matchesCode(0x745982, heroSetter));
   CHECK_NATIVE_PROFILE("HE:M setter body at 0x745993", matchesCode(0x745993, heroSetterBody));
   CHECK_NATIVE_PROFILE("HE:M success at 0x7459F6", matchesCode(0x7459F6, heroSuccess));
   CHECK_NATIVE_PROFILE("HE:M receiver exit at 0x746F00", matchesCode(0x746F00, heroReceiverExit));
   CHECK_NATIVE_PROFILE("HE:M apply failure at 0x74943B", matchesCode(0x74943B, heroApplyFailure));
   CHECK_NATIVE_PROFILE("HE:M invalid path at 0x749645", matchesCode(0x749645, heroInvalid));
   CHECK_NATIVE_PROFILE("UN:J0 entry at 0x733827", matchesCode(0x733827, spellDisabled));
   CHECK_NATIVE_PROFILE("UN:J0 error at 0x733833", matchesCode(0x733833, spellDisabledError));
   CHECK_NATIVE_PROFILE("UN:J0 body at 0x733853", matchesCode(0x733853, spellDisabledBody));
   CHECK_NATIVE_PROFILE("BM:G entry at 0x75F334", matchesCode(0x75F334, battleInfluence));
   CHECK_NATIVE_PROFILE("BM:G success at 0x75F370", matchesCode(0x75F370, battleInfluenceSuccess));
   CHECK_NATIVE_PROFILE("BM:G epilogue at 0x75F85D", matchesCode(0x75F85D, battleInfluenceEpilogue));
   CHECK_NATIVE_PROFILE("SS reset seam at 0x775D98", matchesCode(0x775D98, resetSeam));
   CHECK_NATIVE_PROFILE("SS backup restore at 0x7757BF", matchesCode(0x7757BF, backupRestore));
   CHECK_NATIVE_PROFILE("SS backup capture at 0x77594E", matchesCode(0x77594E, backupCapture));
   CHECK_NATIVE_PROFILE("EA:B creature ability bound at 0x71D8A0", matchesCode(0x71D8A0, creatureAbilityBound));
   CHECK_NATIVE_PROFILE("EA:B creature ability select at 0x71D918", matchesCode(0x71D918, creatureAbilitySelect));
   CHECK_NATIVE_PROFILE("EA:B creature cast at 0x468CCF", matchesCode(0x468CCF, creatureCastSite));
   CHECK_NATIVE_PROFILE("EA:B ordinary bound at 0x71DA5E", matchesCode(0x71DA5E, ordinaryCreatureBound));
   CHECK_NATIVE_PROFILE("EA:B status bound at 0x71DD2D", matchesCode(0x71DD2D, statusCreatureBound));
   CHECK_NATIVE_PROFILE("EA:B info c at 0x721D01", matchesCode(0x721D01, infoCreatureC));
   CHECK_NATIVE_PROFILE("EA:B info p at 0x721EEC", matchesCode(0x721EEC, infoCreatureP));
   CHECK_NATIVE_PROFILE("EA:B info j at 0x7220FC", matchesCode(0x7220FC, infoCreatureJ));
   CHECK_NATIVE_PROFILE("EA:B info a at 0x7222EB", matchesCode(0x7222EB, infoCreatureA));
   CHECK_NATIVE_PROFILE("EA:B info k at 0x7224F4", matchesCode(0x7224F4, infoCreatureK));
   CHECK_NATIVE_PROFILE("EA:B info s at 0x7227C8", matchesCode(0x7227C8, infoCreatureS));
   CHECK_NATIVE_PROFILE("spell-name body at 0x7149C9", matchesCode(0x7149C9, getSpellNameBody));
   CHECK_NATIVE_PROFILE("SS text hook compatibility", getTextCompatible);
   CHECK_NATIVE_PROFILE("spell-name hook compatibility", getNameCompatible);
   CHECK_NATIVE_PROFILE("ERA Apply hook compatibility", hasCompatibleEraApplyHook());
   CHECK_NATIVE_PROFILE("spell-count patch conflict", !_P->GetLastPatchAt(0x402902));
   CHECK_NATIVE_PROFILE("HE:M query patch conflict", !_P->GetLastPatchAt(0x745950));
   CHECK_NATIVE_PROFILE("HE:M setter patch conflict", !_P->GetLastPatchAt(0x745982));
   CHECK_NATIVE_PROFILE("UN:J0 patch conflict", !_P->GetLastPatchAt(0x733827));
   CHECK_NATIVE_PROFILE("BM:G patch conflict", !_P->GetLastPatchAt(0x75F334));
   CHECK_NATIVE_PROFILE("SS reset patch conflict", !_P->GetLastPatchAt(0x775DAC));
   CHECK_NATIVE_PROFILE("EA:B ability threshold patch conflict", !_P->GetLastPatchAt(0x71D8A3));
   CHECK_NATIVE_PROFILE("EA:B ordinary threshold patch conflict", !_P->GetLastPatchAt(0x71DA61));
   CHECK_NATIVE_PROFILE("EA:B status threshold patch conflict", !_P->GetLastPatchAt(0x71DD30));
   CHECK_NATIVE_PROFILE("EA:B ability hook conflict", !_P->GetLastPatchAt(0x71D8A6));
   CHECK_NATIVE_PROFILE("EA:B select hook conflict", !_P->GetLastPatchAt(0x71D918));
   CHECK_NATIVE_PROFILE("EA:B cast hook conflict", !_P->GetLastPatchAt(0x468CCF));
   CHECK_NATIVE_PROFILE("EA:B ordinary hook conflict", !_P->GetLastPatchAt(0x71DA64));
   CHECK_NATIVE_PROFILE("EA:B status hook conflict", !_P->GetLastPatchAt(0x71DD33));
   CHECK_NATIVE_PROFILE("EA:B info c hook conflict", !_P->GetLastPatchAt(0x721D05));
   CHECK_NATIVE_PROFILE("EA:B info p hook conflict", !_P->GetLastPatchAt(0x721EF0));
   CHECK_NATIVE_PROFILE("EA:B info j hook conflict", !_P->GetLastPatchAt(0x722100));
   CHECK_NATIVE_PROFILE("EA:B info a hook conflict", !_P->GetLastPatchAt(0x7222EF));
   CHECK_NATIVE_PROFILE("EA:B info k hook conflict", !_P->GetLastPatchAt(0x7224F8));
   CHECK_NATIVE_PROFILE("EA:B info s hook conflict", !_P->GetLastPatchAt(0x7227CC));

#undef CHECK_NATIVE_PROFILE
   return valid;
}

bool nativeHookRollbackUnsafe = false;

bool installCoreNativeErmHooks()
{
   Patch* hooks[] =
   {
      _PI->CreateLoHook(0x745950, ermHeroSpellQuery),
      _PI->CreateLoHook(0x745982, ermHeroSpellSet),
      _PI->CreateLoHook(0x733827, ermSpellDisabled),
      _PI->CreateLoHook(0x75F334, ermBattleSpellInfluence),
      _PI->CreateLoHook(0x775DAC, restoreSpellDefaultsAfterWogReset),
      _PI->CreateHiHook(0x775702, SPLICE_, EXTENDED_, CDECL_, getNewSpellDefaultText),
      // WoG's three creature-experience receivers use a spell-specific 87
      // boundary. Preserve 0..80 and route every New Spells ID through the
      // explicit allowlists below.
      _PI->CreateBytePatch(0x71D8A3, SPELL_FEAR),
      _PI->CreateBytePatch(0x71DA61, SPELL_FEAR),
      _PI->CreateBytePatch(0x71DD30, SPELL_FEAR),
      _PI->CreateLoHook(0x71D8A6, ermCreatureAbilitySpellBound),
      _PI->CreateLoHook(0x71D918, ermCreatureAbilitySelectSpell),
      _PI->CreateLoHook(0x468CCF, creatureCast),
      _PI->CreateLoHook(0x71DA64, ermOrdinaryCreatureSpellBound),
      _PI->CreateLoHook(0x71DD33, ermStatusCreatureSpellBound),
      _PI->CreateLoHook(0x721D05, ermCreatureSpellInfoBound),
      _PI->CreateLoHook(0x721EF0, ermCreatureSpellInfoBound),
      _PI->CreateLoHook(0x722100, ermCreatureSpellInfoBound),
      _PI->CreateLoHook(0x7222EF, ermCreatureSpellInfoBound),
      _PI->CreateLoHook(0x7224F8, ermCreatureSpellInfoBound),
      _PI->CreateLoHook(0x7227CC, ermCreatureSpellInfoBound),
      _PI->CreateHiHook(0x7149AF, SPLICE_, EXTENDED_, CDECL_, getBoundedWogSpellName)
   };

   const std::size_t hookCount = sizeof(hooks) / sizeof(hooks[0]);
   for (std::size_t i = 0; i < hookCount; ++i)
   {
      if (!hooks[i])
      {
         for (std::size_t j = 0; j < hookCount; ++j)
            if (hooks[j])
               hooks[j]->Destroy();
         return false;
      }
   }

   std::size_t appliedCount = 0;
   for (; appliedCount < hookCount; ++appliedCount)
   {
      hooks[appliedCount]->Apply();
      if (!hooks[appliedCount]->IsApplied())
         break;
   }

   const bool success = appliedCount == hookCount;
   if (success)
      return true;

   // Reverse only patches proven applied, then inspect IsApplied rather than
   // interpreting Patcher_x86's position-valued Undo return as a boolean.
   for (std::size_t i = appliedCount; i > 0; --i)
   {
      Patch* const patch = hooks[i - 1];
      if (patch->IsApplied())
         patch->Undo();
      if (patch->IsApplied())
         nativeHookRollbackUnsafe = true;
   }

   for (std::size_t i = 0; i < hookCount; ++i)
      if (!hooks[i]->IsApplied())
         hooks[i]->Destroy();

   return false;
}

void initHeroAdvInfoEx()
{
   for (int i = 0; i < HEROES_NUM; ++i)
   {
	  heroAdvInfoEx[i].mobilityCastCount = 0;
	  heroAdvInfoEx[i].mobilityWonBattle = true;
	  heroAdvInfoEx[i].eyeOfTheMagiCastCount = 0;
   }
}

bool hasValidHeroId(const hero* Hero)
{
   return Hero && Hero->id >= 0 && Hero->id < HEROES_NUM;
}

int32_t __stdcall queryHeroSpellForAi(const void* combatManager,
   const int32_t casterSide, const int32_t spellId,
   AiSpellStateV1* const outState)
{
   if (!outState || outState->size < sizeof(AiSpellStateV1))
      return 0;

   AiSpellStateV1 result = {};
   result.size = sizeof(result);
   result.mastery = eMasteryInvalid;
   *outState = result;

   CombatManager* const battle =
      const_cast<CombatManager*>(static_cast<const CombatManager*>(combatManager));
   if (!battle || battle != pCombatManager || !pGame ||
       casterSide < ATTACKER || casterSide > DEFENDER ||
       !isDefinedHeroSpell(spellId))
      return 0;

   hero* const Hero = reinterpret_cast<hero*>(battle->hero[casterSide]);
   if (!hasValidHeroId(Hero))
      return 0;

   const int mastery = Hero->get_spell_level(
      static_cast<SpellID>(spellId), battle->spec_terr_type);
   if (mastery < eMasteryNone || mastery > eMasteryExpert)
      return 0;

   const int opponentSide = casterSide == ATTACKER ? DEFENDER : ATTACKER;
   static_assert(sizeof(armyGroup) == sizeof(_Army_),
      "Battle army-group ABI mismatch");
   armyGroup* const opponentArmy =
      reinterpret_cast<armyGroup*>(battle->army[opponentSide]);
   if (!opponentArmy)
      return 0;

   result.known = heroAvailableSpell(Hero, spellId) != 0;
   result.enabled = !pGame->SpellDisabled(static_cast<SpellID>(spellId));
   result.mastery = mastery;
   result.effectiveManaCost = Hero->GetManaCost(
      spellId, opponentArmy, battle->spec_terr_type);
   result.currentMana = Hero->mana;
   *outState = result;
   return 1;
}

const NewSpellsAiInteropV1 newSpellsAiInteropV1 =
{
   sizeof(NewSpellsAiInteropV1),
   NEWSPELLS_AI_INTEROP_ABI_VERSION_V1,
   NEWSPELLS_AI_CAPABILITY_QUERY_HERO_SPELL,
   queryHeroSpellForAi
};

bool publishAiSpellQueryInterop()
{
   if (_P->VarFind(AI_SPELL_QUERY_VARIABLE_NAME))
   {
      Era::WriteLog("NewSpells", "AI spell-query interop",
         "Patcher variable collision; the existing provider was not overwritten and New Spells AI interop remains unpublished.");
      return false;
   }

   const _dword_ providerAddress = static_cast<_dword_>(
      reinterpret_cast<std::uintptr_t>(&newSpellsAiInteropV1));
   Variable* const variable = _P->VarInit(
      AI_SPELL_QUERY_VARIABLE_NAME, providerAddress);
   if (!variable || variable->GetValue() != providerAddress)
   {
      Era::WriteLog("NewSpells", "AI spell-query interop",
         "Could not publish the versioned provider pointer; AI interop remains unavailable.");
      return false;
   }

   return true;
}

bool installFearMeleeGuardHook()
{
   PatcherInstance* const owner = _P->CreateInstance(
      FEAR_MELEE_GUARD_OWNER_NAME);
   if (!owner)
   {
      Era::WriteLog("NewSpells", "Fear melee-guard ownership",
         "Patcher owner collision; the Fear melee guard at 0x422088 was not installed.");
      return false;
   }

   if (!owner->WriteLoHook(0x422088, skipMeleeAttackUnderFear))
   {
      Era::WriteLog("NewSpells", "Fear melee-guard ownership",
         "The dedicated owner was created, but the Fear melee guard at 0x422088 could not be installed.");
      return false;
   }

   return true;
}

int __stdcall magicAnimationBoundsA(LoHook* h, HookContext* c)
{
   c->return_address = c->eax >= 0 && c->eax < activeAnimationCount
      ? 0x4963F0 : 0x496580;
   return NO_EXEC_DEFAULT;
}

int __stdcall magicAnimationBoundsB(LoHook* h, HookContext* c)
{
   c->return_address = c->ecx >= 0 && c->ecx < activeAnimationCount
      ? 0x4965C4 : 0x4967C5;
   return NO_EXEC_DEFAULT;
}

// More Anims replaces the two imm8 compares with LoHooks and owns a sparse
// 3000-record table. Never interpret a jump displacement as an animation
// count. Verify its paired hook/table contract before preserving that table.
bool hasExtendedAnimationLayout(const _MagicAnim_* table)
{
   __try
   {
      const int bounds[] = {0x4963E7, 0x4965BB};
      for (int i = 0; i < 2; ++i)
      {
         Patch* patch = _P->GetLastPatchAt(bounds[i]);
         if (!patch || !patch->IsApplied() || patch->GetType() != LOHOOK_ ||
             patch->GetAddress() != bounds[i] || patch->GetSize() != 9 ||
             !patch->GetOwner() || strcmp(patch->GetOwner(), "z_more_anims") ||
             *reinterpret_cast<const unsigned char*>(bounds[i]) != 0xE9)
            return false;
      }
      uintptr_t cursor = reinterpret_cast<uintptr_t>(table);
      const uintptr_t end = cursor + 3000 * sizeof(*table);
      if (!cursor || end < cursor) return false;
      while (cursor < end)
      {
         MEMORY_BASIC_INFORMATION memory = {};
         if (!VirtualQuery(reinterpret_cast<void*>(cursor), &memory, sizeof(memory)) ||
             memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
         const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
         if (regionEnd <= cursor) return false;
         cursor = regionEnd;
      }
      // These ranges are initialized by More Anims, including reserved
      // records whose DEF files need not exist until another mod provides them.
      return AnimationCatalog::ReservedRowsMatch(table);
   }
   __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int __stdcall afterInit(LoHook* h, HookContext* c)
{
   // The post-initialization boundary closes provider registration
   // immediately. Slots become active only after the canonical WoG spell
   // table has been verified; every later setup failure deactivates them.
   providerRegistrationOpen = false;

   // WoG owns the canonical 200-record table. ERA's relocation registry and
   // the executable pointer cell must identify that same live block.
   void* const originalSpellTable = reinterpret_cast<void*>(0x6854A0);
   void* const registeredSpellTable = Era::GetRealAddr(originalSpellTable);
   _Spell_* const liveSpellTable = o_Spell;
   if (registeredSpellTable == originalSpellTable ||
       registeredSpellTable != liveSpellTable)
   {
      deactivateAllExternalSpellSlots();
      return EXEC_DEFAULT;
   }

   canonicalSpellTable = liveSpellTable;

   // WoG save-table state may still contain a record that belonged to an
   // external provider in a previous session. External slots are owned only
   // by a currently registered and JSON-matched provider.
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      memset(&canonicalSpellTable[spellId], 0, sizeof(_Spell_));
      memset(&wogSpellBackup[spellId], 0, sizeof(_Spell_));
      managedSpellDefaults[spellId] = false;
   }

   sealAndValidateExternalSpellRegistrations();
   nsApplyDataKindTables();

   // Poison
   poisonSpellParams[eMasteryNone].healthModFirstRound = 0.1f;
   poisonSpellParams[eMasteryNone].healthMod = 0.1f;
   poisonSpellParams[eMasteryNone].minHealth = 0.7f;
   poisonSpellParams[eMasteryBasic].healthModFirstRound = 0.1f;
   poisonSpellParams[eMasteryBasic].healthMod = 0.1f;
   poisonSpellParams[eMasteryBasic].minHealth = 0.7f;
   poisonSpellParams[eMasteryAdvanced].healthModFirstRound = 0.2f;
   poisonSpellParams[eMasteryAdvanced].healthMod = 0.1f;
   poisonSpellParams[eMasteryAdvanced].minHealth = 0.6f;
   poisonSpellParams[eMasteryExpert].healthModFirstRound = 0.3f;
   poisonSpellParams[eMasteryExpert].healthMod = 0.1f;
   poisonSpellParams[eMasteryExpert].minHealth = 0.5f;
   
   // Disease
   diseaseSpellParams[eMasteryNone].attackPenalty = 2;
   diseaseSpellParams[eMasteryNone].defensePenalty = 2;
   diseaseSpellParams[eMasteryNone].speedMod = 20;
   diseaseSpellParams[eMasteryBasic].attackPenalty = 2;
   diseaseSpellParams[eMasteryBasic].defensePenalty = 2;
   diseaseSpellParams[eMasteryBasic].speedMod = 20;
   diseaseSpellParams[eMasteryAdvanced].attackPenalty = 4;
   diseaseSpellParams[eMasteryAdvanced].defensePenalty = 4;
   diseaseSpellParams[eMasteryAdvanced].speedMod = 40;
   diseaseSpellParams[eMasteryExpert].attackPenalty = 4;
   diseaseSpellParams[eMasteryExpert].defensePenalty = 4;
   diseaseSpellParams[eMasteryExpert].speedMod = 40;

   // Fear
   fearSpellParams[eMasteryNone].retalDamageMod = 50;
   fearSpellParams[eMasteryBasic].retalDamageMod = 50;
   fearSpellParams[eMasteryAdvanced].retalDamageMod = 25;
   fearSpellParams[eMasteryExpert].retalDamageMod = 0;

   // Magic Animation Table. WoG already appends four animations (83..86) and
   // redirects every table operand. Discover that live table, validate the
   // whole relocation as one unit, preserve it, then append our eight entries.
   const int AnimAddrDefName[] = {0x43F77E, 0x43FB6A, 0x4963FB, 0x4965CF, 0x5A5036, 0x5A6B14, 0x5A7A74, 0x5A962C};
   const int AnimAddrName[] = {0x4689C4, 0x49651A, 0x4966CD, 0x5A6D2D, 0x5A7B06};
   const int previousAnimationBase = *reinterpret_cast<const int*>(AnimAddrDefName[0]);
   const bool extendedAnimationLayout = hasExtendedAnimationLayout(
      reinterpret_cast<const _MagicAnim_*>(previousAnimationBase));
   const int previousAnimationCountA = extendedAnimationLayout ? 3000 :
      *reinterpret_cast<const unsigned char*>(0x4963E9);
   const int previousAnimationCountB = extendedAnimationLayout ? 3000 :
      *reinterpret_cast<const unsigned char*>(0x4965BD);

   // WoG extends the first animation consumer to 87 entries, while the paired
   // original consumer still carries SoD's 83-entry immediate. We replace both
   // upper-bound branches below, so accept either the original or extended
   // immediate at the second site instead of requiring the two bytes to match.
   const int externalAnimationCount = countExternalAnimations();
   const int appendedAnimationCount = 8 + externalAnimationCount;
   bool validAnimationLayout =
      (previousAnimationCountB == ANIM_FEAR ||
       previousAnimationCountB == previousAnimationCountA) &&
      previousAnimationCountA >= ANIM_FEAR &&
       previousAnimationCountA <= ANIMS_MAX - appendedAnimationCount &&
      previousAnimationBase >= 0x10000;

   for (std::size_t i = 0; validAnimationLayout &&
        i < sizeof(AnimAddrDefName) / sizeof(AnimAddrDefName[0]); ++i)
      validAnimationLayout = *reinterpret_cast<const int*>(AnimAddrDefName[i]) == previousAnimationBase;

   for (std::size_t i = 0; validAnimationLayout &&
        i < sizeof(AnimAddrName) / sizeof(AnimAddrName[0]); ++i)
      validAnimationLayout = *reinterpret_cast<const int*>(AnimAddrName[i]) == previousAnimationBase + 4;

   validAnimationLayout = validAnimationLayout &&
      *reinterpret_cast<const int*>(0x43E503) == previousAnimationBase + 8;

   const unsigned char animationBoundsSignatureA[] = {0x85, 0xC0, 0x0F, 0x8C, 0x99, 0x01, 0x00, 0x00};
   const unsigned char animationBoundsSignatureB[] = {0x85, 0xC9, 0x0F, 0x8C, 0x0A, 0x02, 0x00, 0x00};
   validAnimationLayout = validAnimationLayout &&
      memcmp(reinterpret_cast<const void*>(0x4963DF), animationBoundsSignatureA,
         sizeof(animationBoundsSignatureA)) == 0 &&
      memcmp(reinterpret_cast<const void*>(0x4965B3), animationBoundsSignatureB,
         sizeof(animationBoundsSignatureB)) == 0;

   _MagicAnim_* const previousAnimationTable =
      reinterpret_cast<_MagicAnim_*>(previousAnimationBase);
   void* const originalAnimationTable = reinterpret_cast<void*>(0x641E18);
   void* const registeredAnimationTable = Era::GetRealAddr(originalAnimationTable);
   validAnimationLayout = validAnimationLayout &&
      (registeredAnimationTable == originalAnimationTable ||
       registeredAnimationTable == previousAnimationTable || extendedAnimationLayout);

   if (!validAnimationLayout)
   {
      deactivateAllExternalSpellSlots();
      return EXEC_DEFAULT;
   }

   customAnimationBase = previousAnimationCountA;
   activeAnimationCount = customAnimationBase + 8;
   if (!AnimationCatalog::Preserve(SpellAnim, ANIMS_MAX, previousAnimationTable,
          customAnimationBase, appendedAnimationCount))
   {
      deactivateAllExternalSpellSlots();
      return EXEC_DEFAULT;
   }

   SpellAnim[customAnimationBase + 0] = {"C07spE0.def", "Fear", 0x101};
   SpellAnim[customAnimationBase + 1] = {"sp04_.def", "DeathCloud", 1};
   SpellAnim[customAnimationBase + 2] = {"DeathBlow.def", "DeathBlow", 1};
   SpellAnim[customAnimationBase + 3] = {"Toughness.def", "Toughness", 1};
   SpellAnim[customAnimationBase + 4] = {"Claws.def", "Claws", 1};
   SpellAnim[customAnimationBase + 5] = {"C13SPF0.def", "Incineration", 1};
   SpellAnim[customAnimationBase + 6] = {"HourOPow.def", "HourOfPower", 1};
   SpellAnim[customAnimationBase + 7] = {"GldTouch.def", "GoldenTouch", 1};

   int nextAnimation = customAnimationBase + 8;
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      if (!isActiveExternalSpell(spellId))
         continue;
      char* defName = 0;
      char* name = 0;
      int type = 0;
      if (getExternalAnimation(spellId, defName, name, type))
      {
         bool duplicateName = false;
         for (int i = 0; i < nextAnimation; ++i)
            if (SpellAnim[i].name && strcmp(SpellAnim[i].name, name) == 0)
            {
               duplicateName = true;
               break;
            }
         if (duplicateName)
         {
            failExternalSpellClosed(*getExternalSpellSlot(spellId));
            continue;
         }
         SpellAnim[nextAnimation] = {defName, name, type};
         externalAnimationIndex[spellId - NEWSPELLS_EXTERNAL_SPELL_FIRST_ID] =
            nextAnimation++;
      }
   }
   activeAnimationCount = nextAnimation;

   void* const currentAnimationMapping = Era::GetRealAddr(previousAnimationTable);
   if (currentAnimationMapping == previousAnimationTable)
      Era::RedirectMemoryBlock(previousAnimationTable,
         customAnimationBase * sizeof(_MagicAnim_), SpellAnim);
   else if (currentAnimationMapping != SpellAnim)
   {
      deactivateAllExternalSpellSlots();
      return EXEC_DEFAULT;
   }

   // Only mutate operands after every one has been validated.
   for (std::size_t i = 0; i < sizeof(AnimAddrDefName) / sizeof(AnimAddrDefName[0]); ++i)
      _PI->WriteDword(AnimAddrDefName[i], reinterpret_cast<int>(&SpellAnim[0].defName));

   for (std::size_t i = 0; i < sizeof(AnimAddrName) / sizeof(AnimAddrName[0]); ++i)
      _PI->WriteDword(AnimAddrName[i], reinterpret_cast<int>(&SpellAnim[0].name));

   _PI->WriteDword(0x43E503, reinterpret_cast<int>(&SpellAnim[0].type));
   _PI->WriteLoHook(0x4963DF, magicAnimationBoundsA);
   _PI->WriteLoHook(0x4965B3, magicAnimationBoundsB);

   // Resolve each spell's private logical animation key only after the custom
   // block has been appended to WoG's live animation table.
   for (std::size_t i = 0; i < NEW_SPELL_COUNT; ++i)
      fillSpell(newSpell[i]);

   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
      if (isActiveExternalSpell(spellId))
         fillSpell(spellId);

   newSpellDefaultsReady = true;
   restoreNewSpellDefaults(true);

   // Adventure Spells
   initHeroAdvInfoEx();

   if (mobilityRequiresBattle)
      mobilityWinBattleMessage = getLocalizedText("NewSpells.Mobility.WinBattleMessage");
   goldenTouchCombatLogMessage = getLocalizedText("NewSpells.Golden Touch.CombatLogMessage");
   goldenTouchWinBattleMessage = getLocalizedText("NewSpells.Golden Touch.WinBattleMessage");

#ifdef NEWSPELLS_BLIZZARD_NATIVE_PROBE
   typedef int (__stdcall *BlizzardProbe)();
   BlizzardProbe probe = reinterpret_cast<BlizzardProbe>(GetProcAddress(
      GetModuleHandleA("NewSpellsExpansion.dll"), "_RunBlizzardNativeTests@0"));
   if (probe) probe();
#endif
#ifdef NEWSPELLS_BMG_NATIVE_PROBE
   RunBmgNativeProbe();
#endif
#ifdef NEWSPELLS_CEILING_NATIVE_PROBE
   RunCeilingNativeProbe();
#endif

   return EXEC_DEFAULT;
}

// ===============================================================
// ------------------- Availability of spells --------------------
// ---------------------------------------------------------------

// The H3M field is a fixed 70-bit legacy mask. Extended spells are runtime-only
// here and must never change the number of bytes written to the map.
enum { RMG_DISABLED_SPELL_BYTES = CeilDiv(ORIG_SPELLS_NUM, 8) };
static_assert(RMG_DISABLED_SPELL_BYTES == 9,
   "H3M disabled-spell mask must remain exactly 9 bytes");

// Optional
_SpellBitset70_ RMGDisabledSpells;
int __stdcall RMGDisableSpells(LoHook* h, HookContext* c)
{
	for (int iSpell = 0; iSpell < ORIG_SPELLS_NUM; ++iSpell)
	  RMGDisabledSpells.set(iSpell, pGame->SpellDisabled((SpellID)iSpell));

   CALL_3(void, __thiscall, *(int*)(*(int*)c->ebx + 8), c->ebx,
      &RMGDisabledSpells, RMG_DISABLED_SPELL_BYTES);

   c->return_address = 0x54AF1E;
   return NO_EXEC_DEFAULT;
}

int __stdcall RMGDisableSpellsInScrollsA(LoHook* h, HookContext* c)
{
   const std::uintptr_t fieldAddress = static_cast<std::uintptr_t>(c->eax);
   const std::uintptr_t tableField = reinterpret_cast<std::uintptr_t>(&o_Spell->school_flags);
   if (fieldAddress < tableField ||
       (fieldAddress - tableField) % sizeof(_Spell_) != 0)
      return EXEC_DEFAULT;

   enum SpellID spell = static_cast<SpellID>((fieldAddress - tableField) / sizeof(_Spell_));

   if (!isDefinedHeroSpell(spell) ||
       (pGame && pGame->SpellDisabled(spell)))
   {
      c->return_address = 0x5353EE;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall RMGDisableSpellsInScrollsB(LoHook* h, HookContext* c)
{
   const std::uintptr_t fieldAddress = static_cast<std::uintptr_t>(c->eax);
   const std::uintptr_t tableField = reinterpret_cast<std::uintptr_t>(&o_Spell->school_flags);
   if (fieldAddress < tableField ||
       (fieldAddress - tableField) % sizeof(_Spell_) != 0)
      return EXEC_DEFAULT;

   enum SpellID spell = static_cast<SpellID>((fieldAddress - tableField) / sizeof(_Spell_));

   if (!isDefinedHeroSpell(spell) ||
       (pGame && pGame->SpellDisabled(spell)))
   {
      c->return_address = 0x535423;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

void Game::SetSpellsAvailability()
{
   // Preserve the map/UN:J0 state of every valid hero spell. Structural
   // exclusions are derived solely from the live spell record: blank reserve
   // slots and records carrying the native creature-spell flag stay hidden.
   for (int spellId = 0; spellId < activeSpellCount; ++spellId)
      if (!isDefinedHeroSpell(spellId))
         DisableSpell(static_cast<SpellID>(spellId));

   // The H3M trailer supplies the extended part of the map's disabled-spell
   // mask. Only add restrictions here; never force-enable a spell that a map
   // or a previous UN:J0 operation already disabled.
   for (int spellId = 0; spellId < activeSpellCount; ++spellId)
      if (isDefinedHeroSpell(spellId) &&
          NewSpellsMap::IsDisabled(mapDisabledSpells, spellId))
         DisableSpell(static_cast<SpellID>(spellId));
}

void hero::AddSpell(int whichSpell)
{
   if (isDefinedHeroSpell(whichSpell))
   {
      heroInSpellbook(this, whichSpell) = 1;
      heroAvailableSpell(this, whichSpell) = 1;
   }
}

void __fastcall AddSpell(hero* Hero, int unused_edx, int whichSpell)
{
   if (Hero)
      Hero->AddSpell(whichSpell);
}

// Spells which may appear
_SpellBitset_ mayAppearSpellsEx;
int __stdcall expandMayAppearSpells(LoHook* h, HookContext* c)
{
   int mayAppearSpellsAddr;
   int outputBitsetAddr;
   int returnAddr;
   
   switch (h->GetAddress())
   {
   case 0x5BE518:
	  mayAppearSpellsAddr = c->edi + 0xD4;
	  outputBitsetAddr = (int)&c->ecx;
	  returnAddr = 0x5BE51E;
	  break;
   case 0x5BE52D:
	  mayAppearSpellsAddr = c->edi + 0xD4;
	  outputBitsetAddr = (int)&c->eax;
	  returnAddr = 0x5BE533;
	  break;
   default:
	  mayAppearSpellsAddr = c->esi + 0xD4;
	  outputBitsetAddr = (int)&c->ebx;
	  returnAddr = 0x5BEA2A;
   }
      
   _SpellBitset70_* mayAppearSpells = (_SpellBitset70_*)mayAppearSpellsAddr;
      
   for (int iSpell = 0; iSpell < ORIG_SPELLS_NUM; ++iSpell)
	  mayAppearSpellsEx.set(iSpell, mayAppearSpells->test(iSpell)); 

   for (int iSpell = ORIG_SPELLS_NUM; iSpell < SPELLS_NUM; ++iSpell)
	  mayAppearSpellsEx.set(iSpell, pGame->SpellDisabled((SpellID)iSpell));
   
   *(int*)outputBitsetAddr = (int)&mayAppearSpellsEx;
   c->return_address = returnAddr;
   return NO_EXEC_DEFAULT;
}

// Add here *new* spells which must appear in all towns for testing purposes
int mustAppearSpellsList[] = {ID_NONE};

bool mustSpellAppearInTowns(enum SpellID spell)
{
   bool mustAppear = false;

   for (std::size_t i = 0; i < sizeof(mustAppearSpellsList) / sizeof(int); ++i)
   {
      if (spell == mustAppearSpellsList[i])
      {
         mustAppear = true;
         break;
      }
   }

   return mustAppear;
}

// Spells which must appear
_SpellBitset_ mustAppearSpellsEx;
int __stdcall expandMustAppearSpells(LoHook* h, HookContext* c)
{
   _SpellBitset70_* mustAppearSpells = (_SpellBitset70_*)c->eax;
   int spell = c->esi;
   
   for (int iSpell = 0; iSpell < ORIG_SPELLS_NUM; ++iSpell)
	  mustAppearSpellsEx.set(iSpell, mustAppearSpells->test(iSpell)); 

   for (int iSpell = ORIG_SPELLS_NUM; iSpell < SPELLS_NUM; ++iSpell)
	  mustAppearSpellsEx.set(iSpell,
		 !pGame->SpellDisabled((SpellID)iSpell) && mustSpellAppearInTowns((SpellID)iSpell));

   c->return_address = mustAppearSpellsEx.test(spell) ? 0x5BEB3B : 0x5BEB1D;
   return NO_EXEC_DEFAULT;
}
// ===============================================================

int __stdcall initSpells(LoHook* h, HookContext* c)
{
   memcpy(pGame->PField<unsigned char>(4), pGame->PField<unsigned char>(0x4A), ORIG_SPELLS_NUM);
   memset(nsDisabledEx, 0, sizeof(nsDisabledEx));
   pGame->SetSpellsAvailability();
   nsApplyDisabledSpells();
   for (int spell = 0; spell < SPELLS_MAX; ++spell)
      shrineSpells[spell] = pGame->SpellDisabled(static_cast<SpellID>(spell));

   c->return_address = 0x4C2641;
   return NO_EXEC_DEFAULT;
}

int Game::FillShrine(int shrineFlags)
{
   int availSpellsNum = 0;

   for (int i = 0; i < SPELLS_NUM; ++i)
   {
      int lvl = o_Spell[i].level - 1;
      if (lvl >= 0 && lvl < 5 && (1 << lvl) & shrineFlags &&
          o_Spell[i].school_flags && !shrineSpells[i])
         ++availSpellsNum;
   }

   int spell = 0;
   int chosenSpell = 0;

   if (availSpellsNum)
   {
      int rndSpell = Randint(0, availSpellsNum - 1);
     
      for (int i = 0; i < SPELLS_NUM; ++i)
      {
         int lvl = o_Spell[i].level - 1;
         if (lvl >= 0 && lvl < 5 && (1 << lvl) & shrineFlags &&
             o_Spell[i].school_flags && !shrineSpells[i])
         {
            if (spell == rndSpell) break;
            ++spell;
         }
         ++chosenSpell;
      }
     
      shrineSpells[chosenSpell] = true;
   }
   else
   {
      spell = 0;
     
      for (int i = 0; i < SPELLS_NUM; ++i)
      {
         int lvl = o_Spell[i].level - 1;
         if (lvl >= 0 && lvl < 5 && (1 << lvl) & shrineFlags &&
             o_Spell[i].school_flags && !SpellDisabled(static_cast<SpellID>(i)))
         {
            shrineSpells[i] = false;
            ++spell;
         }
      }
     
      chosenSpell = spell ? FillShrine(shrineFlags) : ID_NONE;
   }

   return chosenSpell;
}

int __fastcall FillShrine(Game* game, int unused_edx, int shrineFlags)
{
   return game->FillShrine(shrineFlags);
}

bool army::can_attack(army* target)
{
   if (!target || this == target)
      return false;

   if (spellInfluence[SPELL_BERSERK] || target->spellInfluence[SPELL_BERSERK])
      return true;

   int ownerSide = spellInfluence[SPELL_HYPNOTIZE] ? 1 - this->group : this->group;

   return ownerSide != target->group;
}

bool __fastcall can_attack(army* Army, int unused_edx, army* enemy)
{
   return Army->can_attack(enemy);
}

int army::get_adjusted_defense(army* const enemy, bool frenzy_included)
{
   if (frenzy_included && this->spellInfluence[SPELL_FRENZY])
	  return 0;

   int effDefense = this->sMonInfo.defenseSkill;

   if (enemy)
   {
	  double defenseMod = 1.0;

	  if (enemy->armyType == eCreatureBehemoth)
		 defenseMod = 0.6;

	  if (enemy->armyType == eCreatureAncientBehemoth)
		 defenseMod = 0.2;

	  if (enemy->spellInfluence[SPELL_BEHEMOTHS_CLAWS] && !enemy->can_shoot() &&
	      hasValidArmyCoordinates(enemy))
	  {
		 const int mastery = activeSpellMastery[enemy->group][enemy->index][SPELL_BEHEMOTHS_CLAWS];
		 if (mastery >= eMasteryNone && mastery <= eMasteryExpert)
			defenseMod = o_Spell[SPELL_BEHEMOTHS_CLAWS].effect[mastery] / 100.0;
	  }

	  effDefense = (int)(effDefense * defenseMod);	  
   }

   if (pCombatManager->IsMoatPresent())
   {
	  int secondHexShift = this->facing ? 1 : -1;
	  int second_hex_ix = this->sMonInfo.attributes & CF_DOUBLE_WIDE ? this->gridIndex + secondHexShift : -1;
	  
	  if (pCombatManager->IsInMoat(this->gridIndex, 0) || pCombatManager->IsInMoat(second_hex_ix, 0))
		 effDefense -= 3;
   }

   return max(effDefense, 0);
}

int __stdcall GetEffectiveDefenseAgainst(HiHook* h, army* Army, army* target, bool needFrenzyMod)
{
   if (Army && target && target->spellInfluence[SPELL_BEHEMOTHS_CLAWS] && !target->can_shoot() &&
       hasValidArmyCoordinates(target))
      return Army->get_adjusted_defense(target, needFrenzyMod);
   return CALL_3(int, __thiscall, h->GetDefaultFunc(), Army, target, needFrenzyMod);   
}

// Hour of Power. Bless
double army::get_average_damage() const
{
   double dmg;

   if (this->spellInfluence[SPELL_BLESS] || this->spellInfluence[SPELL_HOUR_OF_POWER])
	  dmg = this->sMonInfo.damageHighBound + this->blessFactor;
   else if (this->spellInfluence[SPELL_CURSE])
	  dmg = max(1, this->sMonInfo.damageLowBound - this->curseFactor);
   else
	  dmg = (this->sMonInfo.damageLowBound + this->sMonInfo.damageHighBound) / 2.0;

   return dmg;
}

double __fastcall get_average_damage(army* Army)
{
   return Army->get_average_damage();
}

int __stdcall getAverageDamageInt(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;

   c->eax = Army->spellInfluence[SPELL_BLESS] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x4424AF;
   return NO_EXEC_DEFAULT;
}

int __stdcall getUnitCombatValue(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;

   c->edx = Army->spellInfluence[SPELL_BLESS] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x4429BA;
   return NO_EXEC_DEFAULT;
}

int __stdcall randomizeBasicDamage(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->ecx;

   c->eax = Army->spellInfluence[SPELL_BLESS] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x442F6A;
   return NO_EXEC_DEFAULT;
}

int __stdcall computeAttackerDamageBonuses(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->ebx;

   c->eax = Army->spellInfluence[SPELL_BLESS] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x443486;
   return NO_EXEC_DEFAULT;
}

int __stdcall getAttackDamageHintString(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;

   c->eax = Army->spellInfluence[SPELL_BLESS] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x492FA5;
   return NO_EXEC_DEFAULT;
}

// Hour of Power. Bloodlust
int __stdcall getAdjustedAttack_Bloodlust(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;

   c->eax = Army->spellInfluence[SPELL_BLOODLUST] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x442160;
   return NO_EXEC_DEFAULT;
}

int __stdcall getOgreMageValue(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->ebx;

   c->eax = Army->spellInfluence[SPELL_BLOODLUST] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x43BF93;
   return NO_EXEC_DEFAULT;
}

// Hour of Power. Fortune
int __stdcall hourOfPower_Fortune(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->edi;

   c->eax = Army->spellInfluence[SPELL_FORTUNE] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   return EXEC_DEFAULT;
}

// Hour of Power. Slayer
int __stdcall getAdjustedAttack_Slayer(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;

   c->eax = Army->spellInfluence[SPELL_SLAYER] || Army->spellInfluence[SPELL_HOUR_OF_POWER];

   c->return_address = 0x442175;
   return NO_EXEC_DEFAULT;
}

float __stdcall SpellCastWorkChance(HiHook* h, CombatManager* combatMgr, int spellId, int casting_side, army* const target_army, bool redirected, bool first_target, bool creature_spell)
{
   ExternalSpellSlot* const external = getExternalSpellSlot(spellId);
   if (external)
   {
      if (!external->active ||
          !(external->descriptor.capabilities & NEWSPELLS_CAP_COMBAT_CAST))
         return 0.0f;

      hero* const Hero = combatMgr && casting_side >= ATTACKER &&
         casting_side <= DEFENDER
         ? reinterpret_cast<hero*>(combatMgr->hero[casting_side]) : 0;
      if ((external->descriptor.flags & NEWSPELLS_PROVIDER_HUMAN_ONLY) &&
          (!Hero || !o_ActivePlayer || !o_ActivePlayer->IsHuman() ||
           Hero->playerOwner != o_ActivePlayer->id))
         return 0.0f;

      if (NsDataSpell* const data = nsDataSpell(spellId))
      {
         if (target_army && nsDataImmune(*data, target_army))
            return 0.0f;
         return CALL_7(float, __thiscall, h->GetDefaultFunc(), combatMgr, spellId, casting_side, target_army, redirected, first_target, creature_spell);
      }

      if (external->descriptor.capabilities & NEWSPELLS_CAP_COMBAT_TARGET)
      {
         if (findPendingExternalCombatTransaction(spellId, casting_side,
                target_army ? target_army->gridIndex : ID_NONE))
            return 1.0f;

         NewSpellsCombatContextV1 context = {};
         context.size = sizeof(context);
         context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         context.combatManager = combatMgr;
         context.casterHero = Hero;
         context.casterStack = combatMgr ? combatMgr->GetActiveStack() : 0;
         context.targetStack = target_army;
         context.spellId = spellId;
         context.casterSide = casting_side;
         context.targetHex = target_army ? target_army->gridIndex : ID_NONE;
         context.mastery = Hero && combatMgr
            ? Hero->get_spell_level(static_cast<SpellID>(spellId),
                                    combatMgr->spec_terr_type)
            : eMasteryNone;
         context.spellPower = Hero ? Hero->stats[2] : 0;
         context.source = creature_spell
            ? NEWSPELLS_SOURCE_CREATURE : NEWSPELLS_SOURCE_HERO;
         const int32_t validation = invokeCombatCallback(*external,
            external->descriptor.ValidateCombatTarget, context);
         if (validation == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*external);
         if (validation != NEWSPELLS_PROVIDER_COMMITTED)
            return 0.0f;
      }

      // The provider owns target validity/resistance semantics. Returning a
      // deterministic full chance prevents a second native decision from
      // overriding an already committed provider validation.
      return 1.0f;
   }

   if (target_army && target_army->spellInfluence[SPELL_HOUR_OF_POWER])
   	  if (spellId == SPELL_BLESS || spellId == SPELL_CURSE || spellId == SPELL_BLOODLUST || spellId == SPELL_SLAYER || spellId == SPELL_FORTUNE || spellId == SPELL_MISFORTUNE)
		 return 0.0f;

   float chance = CALL_7(float, __thiscall, h->GetDefaultFunc(), combatMgr, spellId, casting_side, target_army, redirected, first_target, creature_spell);
   if (chance == 0.0f && target_army &&
       ((spellId == SPELL_DISPEL && target_army->group == casting_side && nsHasActiveDurationEx(target_army, false)) ||
        (spellId == SPELL_DISPEL_HELPFUL_SPELLS && nsHasActiveDurationEx(target_army, true))))
      return 1.0f;
   return chance;
}

void __stdcall beforeNewDayStart(HiHook* h, Game* game)
{
   initHeroAdvInfoEx();
   CALL_1(void, __thiscall, h->GetDefaultFunc(), game);
}

void __stdcall beforeNewGameStart(HiHook* h, Game* game, int a2)
{
   initHeroAdvInfoEx();
   memset(&heroSpellsEx, 0, sizeof(heroSpellsEx));
   memset(&crossoverSpellsEx, 0, sizeof(crossoverSpellsEx));
   loadMapDisabledSpells();
   CALL_2(void, __thiscall, h->GetDefaultFunc(), game, a2);
}

void setupSpellMastery()
{
   memset(&activeSpellMastery, 0, sizeof(activeSpellMastery));
}

void initSpellParams(CombatManager* combatMgr)
{
   if (!combatMgr)
      return;

   setupSpellMastery();
   nsClearDurationsEx();
   nsDataClearMods();

   for (int side = ATTACKER; side <= DEFENDER; ++side)
   {
	  goldenTouchSpell[side].goldForCast = 0;
	  goldenTouchSpell[side].goldForBattle = 0;

      for (int i = 0; i < 21; ++i)
      {
         combatMgr->stack[side][i].Field<float>(0x4C8) = 1.0f;
         slowSpell[side][i].speedMod = 100;
         fearSpell[side][i].speedMod = 100;
         diseaseSpell[side][i].speedMod = 100;
         ageSpell[side][i].healthMod = 100;
		 toughnessSpell[side][i].healthMod = 100;
		 hourOfPowerSpell[side][i].healthMod = 100;
		 explosionSpell[side][i].speedPenalty = false;
      }
   }
}

void notifyExternalBattleLifecycle(CombatManager* const combatMgr,
                                   const int event,
                                   const int winningSide)
{
   NewSpellsBattleContextV1 context = {};
   context.size = sizeof(context);
   context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
   context.combatManager = combatMgr;
   context.event = event;
   context.winningSide = winningSide;
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
      if (slot && slot->active &&
          (slot->descriptor.capabilities & NEWSPELLS_CAP_BATTLE_LIFECYCLE))
      {
         context.spellId = spellId;
         if (invokeBattleCallback(*slot,
                slot->descriptor.OnBattleLifecycle, context) ==
             NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*slot);
      }
   }
}

void __stdcall combatManagerLoadArmies(HiHook* h, CombatManager* combatMgr, bool is_surrounded)
{
   CALL_2(int, __thiscall, h->GetDefaultFunc(), combatMgr, is_surrounded);

   initSpellParams(combatMgr);

   for (int side = ATTACKER; side <= DEFENDER; ++side)
      for (int i = 0; i < 21; ++i)
		 origNumTroops[side][i] = combatMgr->stack[side][i].creature_id != ID_NONE
			? combatMgr->stack[side][i].count_at_start
			: 0;

   notifyExternalBattleLifecycle(combatMgr, NEWSPELLS_BATTLE_START, ID_NONE);
}

void __stdcall combatManagerDoVictory(HiHook* h, CombatManager* combatMgr, int winningGroup)
{
   notifyExternalBattleLifecycle(combatMgr, NEWSPELLS_BATTLE_END, winningGroup);

   for (int side = ATTACKER; side <= DEFENDER; ++side)
      for (int i = 0; i < 21; ++i)
		 combatMgr->stack[side][i].count_at_start = origNumTroops[side][i];

   CALL_2(int, __thiscall, h->GetDefaultFunc(), combatMgr, winningGroup);
}

double __stdcall ComputeAttackerDamageReduction(HiHook* h, army* attacker,
                                                army* defender, bool isRangedAttack)
{
   // Preserve the complete active hook chain. In particular, Game Bug Fixes
   // Extended applies the bad-luck (iLuckStatus == -1) damage multiplier here.
   double result = CALL_3(double, __thiscall, h->GetDefaultFunc(),
                          attacker, defender, isRangedAttack);

   if (!hasValidArmyCoordinates(attacker) ||
       !attacker->spellInfluence[SPELL_FEAR])
      return result;

   const int mastery = activeSpellMastery[attacker->group][attacker->index][SPELL_FEAR];
   if (mastery < eMasteryNone || mastery > eMasteryExpert)
      return result;

   // The default function has already applied Blind/Paralyze as one shared
   // retaliation factor. Replace that factor with Fear only when Fear is the
   // stricter effect; multiplying Fear directly would apply those effects twice.
   double defaultRetaliationFactor = 1.0;
   if (attacker->residualBlindness)
      defaultRetaliationFactor = attacker->blindFactor;

   if (attacker->residualParalyze)
   {
      // Vanilla 0x4438B0 uses Blind's Advanced effect for residual Paralyze.
      const double paralyzeFactor =
         o_Spell[SPELL_BLIND].effect[eMasteryAdvanced] / 100.0;
      if (paralyzeFactor < defaultRetaliationFactor)
         defaultRetaliationFactor = paralyzeFactor;
   }

   const double fearFactor = fearSpellParams[mastery].retalDamageMod / 100.0;
   if (defaultRetaliationFactor > 0.0 && fearFactor < defaultRetaliationFactor)
      result *= fearFactor / defaultRetaliationFactor;

   return result;
}

bool isReadableArmyObject(const army* Army)
{
   if (!Army)
      return false;

   // Real battle stacks are stored in one fixed array. This fast path avoids a
   // VirtualQuery call in the combat and AI hot paths while still checking the
   // pointer numerically before any army field is read.
   if (pCombatManager)
   {
      const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(Army);
      const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(&pCombatManager->stack[0][0]);
      const std::uintptr_t last = first + sizeof(pCombatManager->stack);
      if (address >= first && address < last &&
          (address - first) % sizeof(_BattleStack_) == 0)
      {
         return true;
      }
   }

   // AI evaluators also use heap/stack copies of army records, so membership
   // in CombatManager cannot be required everywhere. Verify that the complete
   // record is readable before accepting coordinates from such a copy.
   MEMORY_BASIC_INFORMATION memoryInfo = {};
   if (VirtualQuery(Army, &memoryInfo, sizeof(memoryInfo)) != sizeof(memoryInfo) ||
       memoryInfo.State != MEM_COMMIT ||
       (memoryInfo.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
   {
      return false;
   }

   const DWORD protection = memoryInfo.Protect & 0xFF;
   if (protection != PAGE_READONLY && protection != PAGE_READWRITE &&
       protection != PAGE_WRITECOPY && protection != PAGE_EXECUTE_READ &&
       protection != PAGE_EXECUTE_READWRITE && protection != PAGE_EXECUTE_WRITECOPY)
   {
      return false;
   }

   const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(Army);
   const std::uintptr_t regionFirst = reinterpret_cast<std::uintptr_t>(memoryInfo.BaseAddress);
   const std::uintptr_t regionLast = regionFirst + memoryInfo.RegionSize;
   return regionLast >= regionFirst && address >= regionFirst && address < regionLast &&
      sizeof(*Army) <= regionLast - address;
}

bool hasValidArmyCoordinates(const army* Army)
{
   return isReadableArmyObject(Army) &&
      Army->group >= ATTACKER && Army->group <= DEFENDER &&
      Army->index >= 0 && Army->index < 21;
}

bool isRealArmy(const army* Army)
{
   if (!Army || !pCombatManager)
      return false;

   const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(Army);
   const std::uintptr_t first = reinterpret_cast<std::uintptr_t>(&pCombatManager->stack[0][0]);
   const std::uintptr_t last = first + sizeof(pCombatManager->stack);

   if (address < first || address >= last ||
       (address - first) % sizeof(_BattleStack_) != 0)
      return false;

   return hasValidArmyCoordinates(Army) &&
      Army == reinterpret_cast<const army*>(&pCombatManager->stack[Army->group][Army->index]);
}

bool hasValidAiSpellInput(const army* Army, const type_enchant_data& data, int casterGroup)
{
   return pCombatManager && hasValidArmyCoordinates(Army) &&
      casterGroup >= ATTACKER && casterGroup <= DEFENDER &&
      Army->numTroops > 0 && Army->origHitPoints > 0 &&
      data.mastery >= eMasteryNone && data.mastery <= eMasteryExpert &&
      data.duration > 0;
}

int saturatingInt64(std::int64_t value)
{
   if (value > (std::numeric_limits<int>::max)())
      return (std::numeric_limits<int>::max)();
   if (value < (std::numeric_limits<int>::min)())
      return (std::numeric_limits<int>::min)();
   return static_cast<int>(value);
}

int saturatingPositiveDouble(double value, int minimum = 0)
{
   if (!(value > static_cast<double>(minimum)))
      return minimum;
   if (value >= static_cast<double>((std::numeric_limits<int>::max)()))
      return (std::numeric_limits<int>::max)();
   return static_cast<int>(value);
}

int normalizedStackHealth(double fullHealth)
{
   if (!(fullHealth >= 1.0))
      return 1;
   if (fullHealth >= 2147483646.0)
      return 0x7FFFFFFF;
   return max(static_cast<int>(fullHealth + 0.95), 1);
}

int __stdcall newRoundSpellSettings(LoHook* h, HookContext* c)
{
   c->return_address = 0x447033;
   army* Army = (army*)c->esi;

   if (!isRealArmy(Army))
      return NO_EXEC_DEFAULT;

   nsNewRoundDurationsEx(Army);

   // Explosion
   if (explosionSpeedReduction && explosionSpell[Army->group][Army->index].speedPenalty)
   {
	  explosionSpell[Army->group][Army->index].speedPenalty = false;
	  ++Army->sMonInfo.speed;
   }

   // Fear
   if (Army->spellInfluence[SPELL_FEAR] && activeSpellMastery[Army->group][Army->index][SPELL_FEAR] == eMasteryExpert)
      Army->retaliationCount = 0;
   
   // Poison
   if (Army->spellInfluence[SPELL_POISON])
   {
      int schoolLevel = activeSpellMastery[Army->group][Army->index][SPELL_POISON];
	  if (schoolLevel < eMasteryNone || schoolLevel > eMasteryExpert)
	     return NO_EXEC_DEFAULT;

	  float healthMul = max(Army->poison_penalty - poisonSpellParams[schoolLevel].healthMod, poisonSpellParams[schoolLevel].minHealth);
	  Army->poison_penalty = healthMul;

      double fullHealth = (double)Army->origHitPoints * healthMul;

      // Poison, Age & Toughness
      if (Army->spellInfluence[SPELL_AGE])
         fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

	  if (Army->spellInfluence[SPELL_TOUGHNESS])
		 fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

	  if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
		 fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
		 fullHealth *= nsDataHealthMul(Army);

      int health = normalizedStackHealth(fullHealth);

	  bool needPoisonAnim = Army->sMonInfo.hitPoints > health;
      if (needPoisonAnim)
		 c->return_address = 0x446FCB;

	  Army->sMonInfo.hitPoints = health;

      if (health - 1 < Army->residualDamage)
         Army->residualDamage = health - 1;
   }
      
   for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
        spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
   {
      ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
      if (!slot || !slot->active ||
          !(slot->descriptor.capabilities & NEWSPELLS_CAP_STATUS_ROUND) ||
          !nsDuration(Army, spellId))
         continue;
      NewSpellsStatusContextV1 context = {};
      context.size = sizeof(context);
      context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
      context.combatManager = pCombatManager;
      context.stack = Army;
      context.spellId = spellId;
      context.mastery = activeSpellMastery[Army->group][Army->index][spellId];
      context.duration = nsDuration(Army, spellId);
      context.event = NEWSPELLS_STATUS_ROUND;
      const int32_t result = invokeStatusCallback(*slot,
         slot->descriptor.OnStatusRound, context);
      if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
         failExternalSpellClosed(*slot);
      else if (result != NEWSPELLS_PROVIDER_COMMITTED)
      {
         // Use the engine removal path so a provider that owns auxiliary
         // status state receives its matching OnStatusRemove notification.
         if (nsDuration(Army, spellId))
            Army->CancelIndividualSpell(spellId);
      }
   }

   return NO_EXEC_DEFAULT;
}

int __stdcall applySpell(LoHook* h, HookContext* c)
{
   int spell = *(int*)(c->ebp + 8);
   hero* Hero = *(hero**)(c->ebp + 0x14);
   army* Army = (army*)c->esi;
   int schoolLevel = c->eax;
   int spellSpecialtyEffect = 0;

   if (spell == SPELL_ANTI_MAGIC)
      nsCancelDurationsEx(Army, true);

   if (!hasValidArmyCoordinates(Army) || spell < 0 || spell >= SPELLS_NUM ||
       schoolLevel < eMasteryNone || schoolLevel > eMasteryExpert)
   {
      c->return_address = 0x444D5C;
      return NO_EXEC_DEFAULT;
   }

   if (spell >= ORIG_SPELLS_NUM && isRealArmy(Army))
      nsFireStackSpell(Army, spell, schoolLevel, true, Hero);

   ExternalSpellSlot* const external = getExternalSpellSlot(spell);
   if (external)
   {
      if (!external->active ||
          !(external->descriptor.capabilities & NEWSPELLS_CAP_STATUS_APPLY))
      {
         nsDuration(Army, spell) = 0;
         activeSpellMastery[Army->group][Army->index][spell] =
            eMasteryNone;
         if (Army->numSpellInfluences > 0)
            --Army->numSpellInfluences;
         // The engine appends the spell ID to SpellInfluenceQueue beginning
         // at 0x444D5C. This influence was never committed, so bypass that
         // insertion and resume at the saved-EDI pop/normal epilogue.
         c->return_address = 0x4450BA;
         return NO_EXEC_DEFAULT;
      }

      NewSpellsStatusContextV1 context = {};
      context.size = sizeof(context);
      context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
      context.combatManager = pCombatManager;
      context.stack = Army;
      context.casterHero = Hero;
      context.spellId = spell;
      context.mastery = schoolLevel;
      context.duration = nsDuration(Army, spell);
      context.event = NEWSPELLS_STATUS_APPLY;
      const int32_t result = invokeStatusCallback(*external,
         external->descriptor.OnStatusApply, context);
      if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
      {
         // This is still before the queue insertion at 0x444D5C. Remove the
         // provisional influence locally first so failExternalSpellClosed()
         // never asks the engine to remove a not-yet-queued effect.
         nsDuration(Army, spell) = 0;
         activeSpellMastery[Army->group][Army->index][spell] =
            eMasteryNone;
         if (Army->numSpellInfluences > 0)
            --Army->numSpellInfluences;
         failExternalSpellClosed(*external);
         c->return_address = 0x4450BA;
         return NO_EXEC_DEFAULT;
      }
      else if (result != NEWSPELLS_PROVIDER_COMMITTED)
      {
         nsDuration(Army, spell) = 0;
         activeSpellMastery[Army->group][Army->index][spell] =
            eMasteryNone;
         if (Army->numSpellInfluences > 0)
            --Army->numSpellInfluences;
         c->return_address = 0x4450BA;
         return NO_EXEC_DEFAULT;
      }
      if (forceCappedDuration[spell])
         nsDuration(Army, spell) = min(forceCappedDuration[spell],
            nsDuration(Army, spell));
      c->return_address = 0x444D5C;
      return NO_EXEC_DEFAULT;
   }

   switch (spell)
   {
   case SPELL_FEAR:
      if (!(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
      {
         fearSpell[Army->group][Army->index].speedMod = o_Spell[SPELL_FEAR].effect[schoolLevel];

         if (schoolLevel == eMasteryExpert)
         {
            Army->CancelIndividualSpell(SPELL_COUNTERSTRIKE);
            Army->retaliationCount = 0;
         }

         int minSpeedMod = 100;

         if (Army->spellInfluence[SPELL_SLOW] && slowSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = slowSpell[Army->group][Army->index].speedMod;

         if (Army->spellInfluence[SPELL_DISEASE] && diseaseSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = diseaseSpell[Army->group][Army->index].speedMod;

         if (fearSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = fearSpell[Army->group][Army->index].speedMod;
               
		 Army->slowPenalty = minSpeedMod / 100.0f;
      }

      break;

   case SPELL_POISON:
      {
         float healthMod = poisonSpellParams[schoolLevel].healthModFirstRound;
		 float healthMul = max(Army->poison_penalty - healthMod, poisonSpellParams[schoolLevel].minHealth);
		 Army->poison_penalty = healthMul;

         double fullHealth = (double)Army->origHitPoints * healthMul;

         // Poison, Age & Toughness
         if (Army->spellInfluence[SPELL_AGE])
            fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
			fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
			fullHealth *= nsDataHealthMul(Army);

         int health = normalizedStackHealth(fullHealth);

		 Army->sMonInfo.hitPoints = health;
         
         if (health - 1 < Army->residualDamage)
            Army->residualDamage = health - 1;
      }
      
	  break;

   case SPELL_DISEASE:
      {
         // Init
         diseaseSpell[Army->group][Army->index].attackPenalty = diseaseSpellParams[schoolLevel].attackPenalty;
         diseaseSpell[Army->group][Army->index].defensePenalty = diseaseSpellParams[schoolLevel].defensePenalty;

         // Optional, as we don't have heroes with Disease spell specialty yet
         if (Hero)
         {
			spellSpecialtyEffect = Hero->GetHeroSpellBonus(SPELL_DISEASE, Army->sMonInfo.level,
			   o_Spell[SPELL_DISEASE].effect[schoolLevel]);
            diseaseSpell[Army->group][Army->index].attackPenalty += spellSpecialtyEffect;
            diseaseSpell[Army->group][Army->index].defensePenalty += spellSpecialtyEffect;
         }

		 Army->sMonInfo.attackSkill -= diseaseSpell[Army->group][Army->index].attackPenalty;
         Army->sMonInfo.defenseSkill -= diseaseSpell[Army->group][Army->index].defensePenalty;

         // Not allowing attack and defense to drop below 0
         if (Army->sMonInfo.attackSkill < 0)
         {
            diseaseSpell[Army->group][Army->index].attackPenalty += Army->sMonInfo.attackSkill;
            Army->sMonInfo.attackSkill = 0;
         }

         if (Army->sMonInfo.defenseSkill < 0)
         {
            diseaseSpell[Army->group][Army->index].defensePenalty += Army->sMonInfo.defenseSkill;
            Army->sMonInfo.defenseSkill = 0;
         }

         if (!(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
         {
            diseaseSpell[Army->group][Army->index].speedMod =
               o_Spell[SPELL_DISEASE].effect[schoolLevel] + spellSpecialtyEffect;

            int minSpeedMod = 100;

            if (Army->spellInfluence[SPELL_SLOW] && slowSpell[Army->group][Army->index].speedMod < minSpeedMod)
               minSpeedMod = slowSpell[Army->group][Army->index].speedMod;

            if (Army->spellInfluence[SPELL_FEAR] && fearSpell[Army->group][Army->index].speedMod < minSpeedMod)
               minSpeedMod = fearSpell[Army->group][Army->index].speedMod;

            if (diseaseSpell[Army->group][Army->index].speedMod < minSpeedMod)
               minSpeedMod = diseaseSpell[Army->group][Army->index].speedMod;
               
			Army->slowPenalty = minSpeedMod / 100.0f;
         }
      }
      
	  break;

   case SPELL_SLOW:
      if (!(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
      {
         Army->CancelIndividualSpell(SPELL_HASTE);
         slowSpell[Army->group][Army->index].speedMod = *(int*)(c->ebp + 0x10);

         int minSpeedMod = 100;

         if (Army->spellInfluence[SPELL_FEAR] && fearSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = fearSpell[Army->group][Army->index].speedMod;

         if (Army->spellInfluence[SPELL_DISEASE] && diseaseSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = diseaseSpell[Army->group][Army->index].speedMod;

         if (slowSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = slowSpell[Army->group][Army->index].speedMod;
               
		 Army->slowPenalty = minSpeedMod / 100.0f;

         // Slow animation
		 Army->sMonFrameInfo.iWalkCycleTime = (int)(Army->origWalkCycleTime * 1.5);
      }
      
	  break;

   case SPELL_AGE:
      {
		 float healthMul = Army->poison_penalty;
         int ageHealthMod = min(o_Spell[SPELL_AGE].effect[schoolLevel], ageSpell[Army->group][Army->index].healthMod);
         ageSpell[Army->group][Army->index].healthMod = ageHealthMod;

         double fullHealth = (double)Army->origHitPoints * healthMul * ageHealthMod / 100.0;
         
		 if (Army->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
			fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
			fullHealth *= nsDataHealthMul(Army);
		 
		 int health = normalizedStackHealth(fullHealth);

         Army->sMonInfo.hitPoints = health;

         if (health - 1 < Army->residualDamage)
            Army->residualDamage = health - 1;
      }
      
	  break;

   case SPELL_DEATH_BLOW:
      break;

   case SPELL_DRAIN_LIFE:
      break;

   case SPELL_TOUGHNESS:
      {
		 float healthMul = Army->poison_penalty;
		 int toughnessHealthMod = o_Spell[SPELL_TOUGHNESS].effect[schoolLevel];
		 toughnessSpell[Army->group][Army->index].healthMod = toughnessHealthMod;

		 double fullHealth = (double)Army->origHitPoints * healthMul * toughnessHealthMod / 100.0;
		 
		 if (Army->spellInfluence[SPELL_AGE])
			fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
			fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
			fullHealth *= nsDataHealthMul(Army);

         int health = normalizedStackHealth(fullHealth);

         Army->sMonInfo.hitPoints = health;

         if (health - 1 < Army->residualDamage)
            Army->residualDamage = health - 1;
      }
      
	  break;

   case SPELL_BEHEMOTHS_CLAWS:
	  break;

   case SPELL_HOUR_OF_POWER:
	  Army->CancelIndividualSpell(SPELL_CURSE);
	  Army->CancelIndividualSpell(SPELL_BLESS);
	  Army->CancelIndividualSpell(SPELL_BLOODLUST);
	  Army->CancelIndividualSpell(SPELL_MISFORTUNE);
	  Army->CancelIndividualSpell(SPELL_FORTUNE);
	  Army->CancelIndividualSpell(SPELL_SLAYER);

	  Army->blessFactor = o_Spell[SPELL_BLESS].effect[eMasteryExpert];

	  Army->bloodlustBonus = o_Spell[SPELL_BLOODLUST].effect[eMasteryExpert];
	  if (Hero)
	  {
		 spellSpecialtyEffect = Hero->GetHeroSpellBonus(SPELL_BLOODLUST, Army->sMonInfo.level, o_Spell[SPELL_BLOODLUST].effect[eMasteryExpert]);
		 Army->bloodlustBonus += spellSpecialtyEffect;
	  }

	  Army->fortuneBonus = o_Spell[SPELL_FORTUNE].effect[eMasteryExpert];
	  if (Hero)
	  {
		 spellSpecialtyEffect = Hero->GetHeroSpellBonus(SPELL_FORTUNE, Army->sMonInfo.level, o_Spell[SPELL_FORTUNE].effect[eMasteryExpert]);
		 Army->fortuneBonus += spellSpecialtyEffect;
	  }

	  Army->slayerLevel = eMasteryExpert;

	  {
		 float healthMul = Army->poison_penalty;
		 int hourOfPowerHealthMod = o_Spell[SPELL_HOUR_OF_POWER].effect[schoolLevel];
		 hourOfPowerSpell[Army->group][Army->index].healthMod = hourOfPowerHealthMod;

		 double fullHealth = (double)Army->origHitPoints * healthMul * hourOfPowerHealthMod / 100.0;

		 if (Army->spellInfluence[SPELL_AGE])
			fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

		 int health = normalizedStackHealth(fullHealth);

		 Army->sMonInfo.hitPoints = health;

		 if (health - 1 < Army->residualDamage)
			Army->residualDamage = health - 1;
	  }

	  break;

   default:
      return EXEC_DEFAULT;
   }

   // Force Capped Duration
   if (forceCappedDuration[spell])
	  nsDuration(Army, spell) = min(forceCappedDuration[spell], nsDuration(Army, spell));
   
   c->return_address = 0x444D5C;
   return NO_EXEC_DEFAULT;
}

bool shouldExecuteExternalDispel(army* const Army, const int spellId)
{
   ExternalSpellSlot* const external = getExternalSpellSlot(spellId);
   if (!external || !external->active ||
       !(external->descriptor.capabilities & NEWSPELLS_CAP_CURE_DISPEL) ||
       !hasValidArmyCoordinates(Army) || nsDuration(Army, spellId) <= 0)
      return true;

   NewSpellsStatusContextV1 context = {};
   context.size = sizeof(context);
   context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
   context.combatManager = pCombatManager;
   context.stack = Army;
   context.spellId = spellId;
   context.mastery = activeSpellMastery[Army->group][Army->index][spellId];
   context.duration = nsDuration(Army, spellId);
   context.event = NEWSPELLS_STATUS_DISPEL;

   const int32_t result = invokeStatusCallback(*external,
      external->descriptor.OnCureOrDispel, context);
   if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
   {
      failExternalSpellClosed(*external);
      // The failed slot has now been removed from every stack. Calling the
      // default remover once more is harmless and preserves any later hook in
      // this call-site chain.
      return true;
   }

   // Both DENIED and CANCELLED are a veto.  Only COMMITTED allows the engine
   // to clear the influence and remove its SpellInfluenceQueue entry.
   return result == NEWSPELLS_PROVIDER_COMMITTED;
}

void __stdcall dispelExternalSpell(HiHook* h, army* Army, int spellId)
{
   if (shouldExecuteExternalDispel(Army, spellId))
      CALL_2(void, __thiscall, h->GetDefaultFunc(), Army, spellId);
}

int __stdcall resetSpell(LoHook* h, HookContext* c)
{
   int spell = *(int*)(c->ebp + 8);
   army* Army = (army*)c->esi;

   if (!hasValidArmyCoordinates(Army) || spell < 0 || spell >= SPELLS_NUM)
   {
      c->return_address = 0x4444E1;
      return NO_EXEC_DEFAULT;
   }

   if (spell >= ORIG_SPELLS_NUM && isRealArmy(Army))
      nsFireStackSpell(Army, spell, activeSpellMastery[Army->group][Army->index][spell], false, 0);

   ExternalSpellSlot* const external = getExternalSpellSlot(spell);
   if (external)
   {
      const int removedMastery =
         activeSpellMastery[Army->group][Army->index][spell];
      activeSpellMastery[Army->group][Army->index][spell] = eMasteryNone;
      if (external->active &&
          (external->descriptor.capabilities & NEWSPELLS_CAP_STATUS_REMOVE))
      {
         NewSpellsStatusContextV1 context = {};
         context.size = sizeof(context);
         context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         context.combatManager = pCombatManager;
         context.stack = Army;
         context.spellId = spell;
         // The engine has already cleared the duration and queue entry at this
         // seam, but New Spells still owns the pre-removal mastery sidecar.
         // Preserve that value for provider teardown before clearing it above.
         context.mastery = removedMastery;
         context.duration = 0;
         context.event = NEWSPELLS_STATUS_REMOVE;
         if (invokeStatusCallback(*external,
                external->descriptor.OnStatusRemove, context) ==
             NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*external);
      }
      c->return_address = 0x4444E1;
      return NO_EXEC_DEFAULT;
   }

   activeSpellMastery[Army->group][Army->index][spell] = eMasteryNone;

   switch (spell)
   {
   case SPELL_FEAR:
      if (!(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
      {
         int minSpeedMod = 100;

         if (Army->spellInfluence[SPELL_SLOW] && slowSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = slowSpell[Army->group][Army->index].speedMod;

         if (Army->spellInfluence[SPELL_DISEASE] && diseaseSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = diseaseSpell[Army->group][Army->index].speedMod;

		 Army->slowPenalty = minSpeedMod / 100.0f;
      }

	  if (isRealArmy(Army))
		 fearSpell[Army->group][Army->index].speedMod = 100;
      
	  break;

   case SPELL_POISON:
      {
		 Army->poison_penalty = 1.0f;
         double fullHealth = (double)Army->origHitPoints;

         // Poison, Age & Toughness
         if (Army->spellInfluence[SPELL_AGE])
            fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
			fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
			fullHealth *= nsDataHealthMul(Army);

         int health = normalizedStackHealth(fullHealth);

         Army->sMonInfo.hitPoints = health;

         if (health - 1 < Army->residualDamage)
            Army->residualDamage = health - 1;
      }
      break;

   case SPELL_DISEASE:
      Army->sMonInfo.attackSkill += diseaseSpell[Army->group][Army->index].attackPenalty;
      Army->sMonInfo.defenseSkill += diseaseSpell[Army->group][Army->index].defensePenalty;

      if (!(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
      {
         int minSpeedMod = 100;

         if (Army->spellInfluence[SPELL_SLOW] && slowSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = slowSpell[Army->group][Army->index].speedMod;

         if (Army->spellInfluence[SPELL_FEAR] && fearSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = fearSpell[Army->group][Army->index].speedMod;

		 Army->slowPenalty = minSpeedMod / 100.0f;
      }
      
	  if (isRealArmy(Army))
		 diseaseSpell[Army->group][Army->index].speedMod = 100;

      break;
   
   case SPELL_SLOW:
      if (!(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
      {
         int minSpeedMod = 100;

         if (Army->spellInfluence[SPELL_FEAR] && fearSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = fearSpell[Army->group][Army->index].speedMod;

         if (Army->spellInfluence[SPELL_DISEASE] && diseaseSpell[Army->group][Army->index].speedMod < minSpeedMod)
            minSpeedMod = diseaseSpell[Army->group][Army->index].speedMod;

		 Army->slowPenalty = minSpeedMod / 100.0f;
      }

      // Slow animation
	  Army->sMonFrameInfo.iWalkCycleTime = Army->origWalkCycleTime; 

      if (isRealArmy(Army))
		 slowSpell[Army->group][Army->index].speedMod = 100;

      break;

   case SPELL_AGE:
      {
		 float healthMul = Army->poison_penalty;
         
		 if (isRealArmy(Army))
			ageSpell[Army->group][Army->index].healthMod = 100;

         double fullHealth = (double)Army->origHitPoints * healthMul;
		 
		 if (Army->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
			fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
			fullHealth *= nsDataHealthMul(Army);

         int health = normalizedStackHealth(fullHealth);

         Army->sMonInfo.hitPoints = health;

         if (health - 1 < Army->residualDamage)
            Army->residualDamage = health - 1;
      }
      break;

   case SPELL_DEATH_BLOW:
      break;

   case SPELL_DRAIN_LIFE:
      break;

   case SPELL_TOUGHNESS:
      {
		 float healthMul = Army->poison_penalty;
         
		 if (isRealArmy(Army))
			toughnessSpell[Army->group][Army->index].healthMod = 100;

         double fullHealth = (double)Army->origHitPoints * healthMul;
		 
		 if (Army->spellInfluence[SPELL_AGE])
			fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_HOUR_OF_POWER])
			fullHealth *= hourOfPowerSpell[Army->group][Army->index].healthMod / 100.0;
			fullHealth *= nsDataHealthMul(Army);

         int health = normalizedStackHealth(fullHealth);

         Army->sMonInfo.hitPoints = health;

         if (health - 1 < Army->residualDamage)
            Army->residualDamage = health - 1;
      }
      break;

   case SPELL_BEHEMOTHS_CLAWS:
	  break;


   case SPELL_HOUR_OF_POWER:
	  {
		 float healthMul = Army->poison_penalty;
         
		 if (isRealArmy(Army))
			hourOfPowerSpell[Army->group][Army->index].healthMod = 100;

         double fullHealth = (double)Army->origHitPoints * healthMul;
		 
		 if (Army->spellInfluence[SPELL_AGE])
			fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

		 if (Army->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

         int health = normalizedStackHealth(fullHealth);

         Army->sMonInfo.hitPoints = health;

         if (health - 1 < Army->residualDamage)
            Army->residualDamage = health - 1;
      }
	  break;

   default:
      return EXEC_DEFAULT;
   }

   c->return_address = 0x4444E1;
   return NO_EXEC_DEFAULT;
}

army* findBattleStackAtHex(const int hex)
{
   if (!pCombatManager || hex < 0)
      return 0;
   for (int side = ATTACKER; side <= DEFENDER; ++side)
      for (int index = 0; index < 21; ++index)
      {
         army* const Army = reinterpret_cast<army*>(
            &pCombatManager->stack[side][index]);
         if (isRealArmy(Army) && Army->gridIndex == hex)
            return Army;
      }
   return 0;
}

int __stdcall creatureCast(LoHook* h, HookContext* c)
{
   int spell = c->eax;
   int hex_ix = *(int*)(c->esi - 0xB4);

   ExternalSpellSlot* const external = getExternalSpellSlot(spell);
   if (external)
   {
      if (external->active &&
          (external->descriptor.capabilities & NEWSPELLS_CAP_CREATURE_CAST))
      {
         army* const caster = pCombatManager
            ? pCombatManager->GetActiveStack() : 0;
         NewSpellsCombatContextV1 context = {};
         context.size = sizeof(context);
         context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         context.combatManager = pCombatManager;
         context.casterStack = caster;
         context.targetStack = findBattleStackAtHex(hex_ix);
         context.spellId = spell;
         context.casterSide = caster ? caster->group : ID_NONE;
         context.targetHex = hex_ix;
         context.mastery = eMasteryNone;
         context.spellPower = 3;
         context.source = NEWSPELLS_SOURCE_CREATURE;
         const int32_t result = invokeCombatCallback(*external,
            external->descriptor.OnCreatureCast, context);
         if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*external);
      }
      c->return_address = 0x468CE7;
      return NO_EXEC_DEFAULT;
   }

   switch (spell)
   {
   case SPELL_POISON:
   case SPELL_DISEASE:
	case SPELL_FEAR:
	case SPELL_EXPLOSION:
	case SPELL_GOLDEN_TOUCH:
	  pCombatManager->CastSpell(spell, hex_ix, 1, -1, eMasteryNone, 3); 
      break;
   case SPELL_AGE:
	  pCombatManager->CastSpell(spell, hex_ix, 1, -1, eMasteryExpert, 3); 
      break;
   default:
      return EXEC_DEFAULT;
   }
   
   c->return_address = 0x468CE7;
   return NO_EXEC_DEFAULT;
}


int __stdcall zombieDiseaseOnMagicPlains(LoHook* h, HookContext* c)
{
   int spell = *(int*)(c->ebp + 8);

   ExternalSpellSlot* const external = getExternalSpellSlot(spell);
   if (external)
   {
      if (!external->active ||
          !(external->descriptor.capabilities & NEWSPELLS_CAP_COMBAT_CAST) ||
          !pCombatManager)
      {
         c->return_address = 0x5A2A9A;
         return NO_EXEC_DEFAULT;
      }

      const int casterKind = *reinterpret_cast<int*>(c->ebp + 0x10);
      const int side = pCombatManager->current_side;
      hero* const Hero = *reinterpret_cast<hero**>(c->ebp - 0x14);
      if ((external->descriptor.flags & NEWSPELLS_PROVIDER_HUMAN_ONLY) &&
          (!Hero || !o_GameMgr || !o_GameMgr->GetPlayer(Hero->playerOwner) ||
           !o_GameMgr->GetPlayer(Hero->playerOwner)->IsHuman()))
      {
         c->return_address = 0x5A2A9A;
         return NO_EXEC_DEFAULT;
      }

      if (external->descriptor.capabilities & NEWSPELLS_CAP_COMBAT_TARGET)
      {
         const int targetHex = *reinterpret_cast<int*>(c->ebp + 0x0C);
         NewSpellsCombatContextV1 context = {};
         context.size = sizeof(context);
         context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         context.combatManager = pCombatManager;
         context.casterHero = Hero;
         context.casterStack = pCombatManager->GetActiveStack();
         context.targetStack = findBattleStackAtHex(targetHex);
         context.spellId = spell;
         context.casterSide = side;
         context.targetHex = targetHex;
         context.mastery = *reinterpret_cast<int*>(c->ebp - 0x44);
         context.spellPower = *reinterpret_cast<int*>(c->ebp + 0x1C);
         context.source = casterKind == 0 ? NEWSPELLS_SOURCE_HERO :
            NEWSPELLS_SOURCE_CREATURE;
         const int32_t validation = invokeCombatCallback(*external,
            external->descriptor.ValidateCombatTarget, context);
         if (validation == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*external);
         if (validation != NEWSPELLS_PROVIDER_COMMITTED)
         {
            c->return_address = 0x5A2A9A;
            return NO_EXEC_DEFAULT;
         }
      }

      if (!beginExternalCombatTransaction(c, spell))
      {
         c->return_address = 0x5A2A9A;
         return NO_EXEC_DEFAULT;
      }
   }
   
   if (pCombatManager->Field<int>(0x53C0) == 1 && spell == SPELL_DISEASE)
   {
      c->esi = 1;
      *(int*)(c->ebp - 0x44) = 1;
   }
      
   return EXEC_DEFAULT;
}

int __stdcall cureNewSpells(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;

   Army->CancelIndividualSpell(SPELL_FEAR);

   if (hasValidArmyCoordinates(Army))
   {
      for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
           spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
      {
         ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
         if (!slot || !slot->active ||
             !(slot->descriptor.capabilities & NEWSPELLS_CAP_CURE_DISPEL) ||
             !nsDuration(Army, spellId))
            continue;
         NewSpellsStatusContextV1 context = {};
         context.size = sizeof(context);
         context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         context.combatManager = pCombatManager;
         context.stack = Army;
         context.spellId = spellId;
         context.mastery = activeSpellMastery[Army->group][Army->index][spellId];
         context.duration = nsDuration(Army, spellId);
         context.event = NEWSPELLS_STATUS_CURE;
         const int32_t result = invokeStatusCallback(*slot,
            slot->descriptor.OnCureOrDispel, context);
         if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*slot);
         else if (result == NEWSPELLS_PROVIDER_COMMITTED)
            Army->CancelIndividualSpell(spellId);
      }
   }
   
   return EXEC_DEFAULT;
}

bool isDrainLifeImmune(int unitType)
{
   switch (unitType)
   {
   case eCreatureStoneGargoyle:
   case eCreatureObsidianGargoyle:
   case eCreatureStoneGolem:
   case eCreatureIronGolem:
   case eCreatureGoldGolem:
   case eCreatureDiamondGolem:
   case eCreatureGiant:
   case eCreatureTitan:
   case eCreatureVampireLord:
   case eCreatureAirElemental:
   case eCreatureStormElemental:
   case eCreatureEarthElemental:
   case eCreatureMagmaElemental:
   case eCreatureFireElemental:
   case eCreatureEnergyElemental:
   case eCreatureWaterElemental:
   case eCreatureIceElemental:
   case eCreaturePsychicElemental:
   case eCreatureMagicElemental:
      return true;
   default:
      return false;
   }
}

bool isSupportiveUnitNotBallista(int unitType)
{
   return unitType == eCreatureCatapult || unitType == eCreatureFirstAidTent || unitType == eCreatureAmmoCart;
}

int __stdcall getStackMagicVulnerability(LoHook* h, HookContext* c)
{
   _CreatureInfo_* unit = (_CreatureInfo_*)c->edi;
   int unitType = *(int*)(c->ebp - 8);
   int spell = *(int*)(c->ebp - 0x10);

   bool immune = false; 

   switch (spell)
   {
   // Let's allow to cast Disease on battle machines
   case SPELL_DISEASE:
      if (!(unit->flags & CF_ALIVE) && !(unit->flags & CF_SIEGE_WEAPON))
         immune = true;
      break;
   case SPELL_AGE:
      if (!(unit->flags & CF_ALIVE))
         immune = true;
      break;
   case SPELL_FEAR:
      if (!(unit->flags & CF_ALIVE))
         immune = true;
      break;
   case SPELL_DEATH_CLOUD_NEW:
	  if (unit->flags & CF_UNDEAD)
		 immune = true;
	  break;
   case SPELL_DEATH_BLOW:
      break;
   case SPELL_DRAIN_LIFE:
      if (isDrainLifeImmune(unitType))
         immune = true;
      break;
   case SPELL_BEHEMOTHS_CLAWS:
	  if (unitType == eCreatureAncientBehemoth)
		 immune = true;
	  break;
   case SPELL_HOUR_OF_POWER:
	  if (unit->flags & CF_UNDEAD)
		 immune = true;
	  break;
   case SPELL_BLESS:
	  if (unit->flags & CF_UNDEAD)
		 immune = true;
	  break;

   default:
      return EXEC_DEFAULT;
   }

   c->return_address = immune ? 0x44A3E8 : 0x44A416;
   return NO_EXEC_DEFAULT;
}

int hero::getBestMagicSchoolForSpell(SpellID spell)
{
   if (spell < 0 || spell >= SPELLS_NUM)
      return HSS_EARTH_MAGIC;

   const int schoolFlags = o_Spell[spell].school_flags;
   int bestSchool = HSS_EARTH_MAGIC;
   int bestExpertise = -1;

   if (schoolFlags & SSF_EARTH)
   {
      bestSchool = HSS_EARTH_MAGIC;
      bestExpertise = this->SSLevel[HSS_EARTH_MAGIC];
   }
   if ((schoolFlags & SSF_AIR) && this->SSLevel[HSS_AIR_MAGIC] > bestExpertise)
   {
      bestSchool = HSS_AIR_MAGIC;
      bestExpertise = this->SSLevel[HSS_AIR_MAGIC];
   }
   if ((schoolFlags & SSF_FIRE) && this->SSLevel[HSS_FIRE_MAGIC] > bestExpertise)
   {
      bestSchool = HSS_FIRE_MAGIC;
      bestExpertise = this->SSLevel[HSS_FIRE_MAGIC];
   }
   if ((schoolFlags & SSF_WATER) && this->SSLevel[HSS_WATER_MAGIC] > bestExpertise)
      bestSchool = HSS_WATER_MAGIC;

   return bestSchool;
}

// ===============================================================
// ---- Summon Sprite, Summon Magic Elemental, Summon Firebird ---
// ---------------------------------------------------------------
int __fastcall get_elemental_type(enum SpellID spell)
{
   int unitType;

   switch (spell)
   {
   case SPELL_SUMMON_FIRE_ELEMENTAL:
      unitType = eCreatureFireElemental;
      break;
   case SPELL_SUMMON_EARTH_ELEMENTAL:
      unitType = eCreatureEarthElemental;
      break;
   case SPELL_SUMMON_WATER_ELEMENTAL:
      unitType = eCreatureWaterElemental;
      break;
   case SPELL_SUMMON_AIR_ELEMENTAL:
      unitType = eCreatureAirElemental;
      break;
   case SPELL_SUMMON_SPRITE:
      unitType = eCreatureSprite;
      break;
   case SPELL_SUMMON_MAGIC_ELEMENTAL:
      unitType = eCreatureMagicElemental;
      break;
   case SPELL_SUMMON_FIREBIRD:
      unitType = eCreatureFirebird;
      break;
   default:
      unitType = nsDataSummonCreature(spell);
   }

   return unitType;
}

double getSummoningEffect(enum SpellID spell, int schoolLevel)
{
   if (spell < 0 || spell >= SPELLS_NUM ||
       schoolLevel < eMasteryNone || schoolLevel > eMasteryExpert)
      return 0.0;

   double effect = 0.0;
   double elem_AI_Value_Avg = (o_pCreatureInfo[eCreatureAirElemental].AI_value + o_pCreatureInfo[eCreatureEarthElemental].AI_value +
      o_pCreatureInfo[eCreatureFireElemental].AI_value + o_pCreatureInfo[eCreatureWaterElemental].AI_value) / 4.0;

   switch (spell)
   {
   case SPELL_SUMMON_FIRE_ELEMENTAL:
   case SPELL_SUMMON_EARTH_ELEMENTAL:
   case SPELL_SUMMON_WATER_ELEMENTAL:
   case SPELL_SUMMON_AIR_ELEMENTAL:
      effect = 1;
      break;
   case SPELL_SUMMON_SPRITE:
      if (o_pCreatureInfo[eCreatureSprite].AI_value > 0)
         effect = 0.8 * elem_AI_Value_Avg / o_pCreatureInfo[eCreatureSprite].AI_value;
      break;
   case SPELL_SUMMON_MAGIC_ELEMENTAL:
      if (o_pCreatureInfo[eCreatureMagicElemental].AI_value > 0)
         effect = 1.2 * elem_AI_Value_Avg / o_pCreatureInfo[eCreatureMagicElemental].AI_value;
      break;
   case SPELL_SUMMON_FIREBIRD:
      if (o_pCreatureInfo[eCreatureFirebird].AI_value > 0)
         effect = 1.2 * elem_AI_Value_Avg / o_pCreatureInfo[eCreatureFirebird].AI_value;
      break;
   default:
      return 0.0;
   }

   effect *= o_Spell[spell].effect[schoolLevel];

   return effect;
}

bool CombatManager::AbleToSummonElemental(SpellID spell, long side)
{
   if (side < ATTACKER || side > DEFENDER)
      return false;

   if (stacks_count[side] >= 20)
	  return false;

   int unitType = PField<int>(0x132A8)[side];
   if (unitType == ID_NONE)
      return true;

   if (NsDataSpell* const data = nsDataSpell(spell))
      return data->kind == NS_KIND_SUMMON && (!data->summonExclusive || unitType == data->summonCreature);

   bool result;

   switch (spell)
   {
   case SPELL_SUMMON_FIRE_ELEMENTAL:
      result = unitType == eCreatureFireElemental;
      break;
   case SPELL_SUMMON_EARTH_ELEMENTAL:
      result = unitType == eCreatureEarthElemental;
      break;
   case SPELL_SUMMON_WATER_ELEMENTAL:
      result = unitType == eCreatureWaterElemental;
      break;
   case SPELL_SUMMON_AIR_ELEMENTAL:
      result = unitType == eCreatureAirElemental;
      break;
   case SPELL_SUMMON_SPRITE:
      result = unitType == eCreatureSprite;
      break;
   case SPELL_SUMMON_MAGIC_ELEMENTAL:
      result = unitType == eCreatureMagicElemental;
      break;
   case SPELL_SUMMON_FIREBIRD:
      result = unitType == eCreatureFirebird;
      break;
   default:
      result = unitType == ID_NONE;
   }

   return result;
}

bool __fastcall AbleToSummonElemental(CombatManager* combatMgr, int unused_edx, enum SpellID spell, int side)
{
   return combatMgr && combatMgr->AbleToSummonElemental(spell, side);   
}

int __stdcall SummonCreatures(LoHook* h, HookContext* c)
{
   int spell = *(int*)(c->ebp + 8);
   int unitType;

   switch (spell)
   {
   case SPELL_SUMMON_SPRITE:
      unitType = eCreatureSprite;
      break;
   case SPELL_SUMMON_MAGIC_ELEMENTAL:
      unitType = eCreatureMagicElemental;
      break;
   case SPELL_SUMMON_FIREBIRD:
      unitType = eCreatureFirebird;
      break;
   default:
      return EXEC_DEFAULT;
   }

   if (!pCombatManager || c->esi < eMasteryNone || c->esi > eMasteryExpert)
   {
      c->return_address = 0x5A2368;
      return NO_EXEC_DEFAULT;
   }

   CALL_5(void, __thiscall, 0x5A7390, pCombatManager, spell, unitType, *(int*)(c->ebp + 0x1C), c->esi);

   c->return_address = 0x5A2368;
   return NO_EXEC_DEFAULT;
}

int __stdcall skipNonElementals(LoHook* h, HookContext* c)
{
   int unitType = *(int*)(c->ebp + 0xC);

   NsDataSpell* const data = nsDataSpell(*(int*)(c->ebp + 8));
   if (data && data->kind == NS_KIND_SUMMON && !data->summonExclusive)
   {
      c->return_address = 0x5A74DD;
      return NO_EXEC_DEFAULT;
   }
   
   switch (unitType)
   {
   case eCreatureSprite:
   case eCreatureFirebird:
      c->return_address = 0x5A74DD;
      return NO_EXEC_DEFAULT;
   default:
      return EXEC_DEFAULT;
   }
}

int spriteSpellParams[] = {1000, 1000, 1500, 2000};
int magicElementalSpellParams[] = {100, 100, 150, 200};
int firebirdSpellParams[] = {50, 50, 75, 100};

int __stdcall setSummonedCreaturesNumber(LoHook* h, HookContext* c)
{
   int unitType = *(int*)(c->ebp + 0xC);
   int spellPower = *(int*)(c->ebp + 0x10);
   int schoolLevel = *(int*)(c->ebp + 0x14);
   const int* spellParams = 0;

   switch (unitType)
   {
   case eCreatureSprite:
      spellParams = spriteSpellParams;
      break;
   case eCreatureMagicElemental:
      spellParams = magicElementalSpellParams;
      break;
   case eCreatureFirebird:
      spellParams = firebirdSpellParams;
      break;
   default:
      return EXEC_DEFAULT;
   }

   if (schoolLevel < eMasteryNone || schoolLevel > eMasteryExpert || spellPower <= 0)
      c->esi = 1;
   else
      c->esi = saturatingInt64(
         static_cast<std::int64_t>(spellParams[schoolLevel]) * spellPower / 100);

   if (static_cast<int>(c->esi) < 1)
      c->esi = 1;
   
   c->ecx = c->ebx;
   c->return_address = 0x5A7526;
   return NO_EXEC_DEFAULT;
}

int __stdcall checkSummonedCreaturesType(LoHook* h, HookContext* c)
{
   c->return_address = 0x59F8B7;
   int spell = c->ebx;
     
   switch (spell)
   {
   case SPELL_SUMMON_SPRITE:
   case SPELL_SUMMON_FIREBIRD:
      c->return_address = 0x59F8C0;
      break;
   case SPELL_SUMMON_FIRE_ELEMENTAL:
      c->eax = eCreatureFireElemental;
      break;
   case SPELL_SUMMON_EARTH_ELEMENTAL:
      c->eax = eCreatureEarthElemental;
      break;
   case SPELL_SUMMON_WATER_ELEMENTAL:
      c->eax = eCreatureWaterElemental;
      break;
   case SPELL_SUMMON_AIR_ELEMENTAL:
      c->eax = eCreatureAirElemental;
      break;
   case SPELL_SUMMON_MAGIC_ELEMENTAL:
      c->eax = eCreatureMagicElemental;
      break;
   default:
      c->eax = ID_NONE;
   }
      
   return NO_EXEC_DEFAULT;
}
// ===============================================================

// ===============================================================
// --------------------- AI Spell Weighting ----------------------
// ---------------------------------------------------------------

// Debug
#ifdef NEWSPELLS_DEBUG
void showSpellInfo(SpellID spell, const type_AI_spellcaster* const caster, army* const Army1, army* const Army2, type_enchant_data data, int valueParam, int value)
{
   debugStr("{%s} evaluates {%s}\n\n{%d} %s\n{%d} %s\n\nskillMastery = {%d}\nspellDuration = {%d}\nlowest_attack = {%d}\nlowest_defense = {%d}\n\n" \
	  "valueParam = {%d}\nvalue = {%d}",
	  caster->current_hero ? caster->current_hero->name : "Army",
	  o_Spell[spell].name,
	  Army1->numTroops,
	  Army1->numTroops > 1 ? Army1->sMonInfo.m_plural_name : Army1->sMonInfo.m_name,
	  Army2 ? Army2->numTroops : 0,
	  Army2 ? (Army2->numTroops > 1 ? Army2->sMonInfo.m_plural_name : Army2->sMonInfo.m_name) : 0,
	  data.mastery,
	  data.duration,
	  caster->estimate.lowest_attack,
	  caster->estimate.lowest_defense,
	  valueParam,
	  value);
}
#endif

// Anti-Magic AI support from the upstream 1.03 RC2 source. The ERA port had
// dropped it, causing the stock 70-spell evaluator to run against the expanded
// influence table.
int type_AI_spellcaster::get_antimagic_cancel_value(army* currentArmy, TSkillMastery mastery) const
{
   if (!hasValidArmyCoordinates(currentArmy) || mastery < eMasteryNone || mastery > eMasteryExpert)
      return 0;

   int spellValueSum = 0;
   int dispelLevel = 3;

   if (mastery == eMasteryAdvanced)
      dispelLevel = 4;
   else if (mastery == eMasteryExpert)
      dispelLevel = 5;

   for (int spellId = SPELL_SHIELD; spellId < SPELLS_NUM; ++spellId)
   {
      int (__thiscall type_AI_spellcaster::*valueFunction)(army* const, type_enchant_data) const =
         get_enchantment_function(static_cast<SpellID>(spellId));

      if (!nsDuration(currentArmy, spellId) || !valueFunction ||
          dispelLevel < o_Spell[spellId].level)
         continue;

      type_enchant_data enchantData = {};
      enchantData.spell = static_cast<SpellID>(spellId);
      enchantData.mastery = static_cast<TSkillMastery>(
         activeSpellMastery[currentArmy->group][currentArmy->index][spellId]);
      enchantData.power = nsDuration(currentArmy, spellId);
      enchantData.duration = nsDuration(currentArmy, spellId);
      enchantData.check_resistance = false;

      currentArmy->CancelIndividualSpell(spellId);

      const bool affectsOurSide = currentArmy->group == our_group;
      const bool harmfulSpell = o_Spell[spellId].type < 0;
      if (enemy_caster && harmfulSpell == affectsOurSide)
         spellValueSum += (enemy_caster->*valueFunction)(currentArmy, enchantData);
   }

   return spellValueSum;
}

int type_AI_spellcaster::get_antimagic_value(army* const Army, type_enchant_data data) const
{
   if (!Army || data.mastery < eMasteryNone || data.mastery > eMasteryExpert)
      return 0;

   army* currentArmy = new army;
   currentArmy->copyConstructor(Army);
   const int cancelValue = get_antimagic_cancel_value(currentArmy, data.mastery);
   delete currentArmy;

   return cancelValue + get_protection_value(Army, eSchoolAll,
      o_Spell[SPELL_ANTI_MAGIC].effect[data.mastery], data.duration, 0);
}

// Poison
int type_AI_spellcaster::get_poison_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group))
      return 0;

   int value = 0;
   int valueParam = 0;

   if (!Army->spellInfluence[SPELL_POISON] && !this->win_likely)
   {
	  float workChance = pCombatManager->SpellCastWorkChance(SPELL_POISON, this->our_group, Army, false, true, this->is_creature_spell);
	  if (workChance > 0.0f)
	  {
		 army* dummyArmy = new army;
		 dummyArmy->copyConstructor(Army);

		 int healthBefore = dummyArmy->currentHealth();
         
		 // First round
		 float healthMod = poisonSpellParams[data.mastery].healthModFirstRound;
		 float healthMul = max(dummyArmy->poison_penalty - healthMod, poisonSpellParams[data.mastery].minHealth);
		 dummyArmy->poison_penalty = healthMul;
		 double fullHealth = (double)dummyArmy->origHitPoints * healthMul;

		 // Poison, Age & Toughness
		 if (dummyArmy->spellInfluence[SPELL_AGE])
			fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

		 if (dummyArmy->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

		 int health = normalizedStackHealth(fullHealth);

		 dummyArmy->sMonInfo.hitPoints = health;
         
		 if (health - 1 < dummyArmy->residualDamage)
			dummyArmy->residualDamage = health - 1;

		 int healthAfter = dummyArmy->currentHealth();
		 int damage = healthBefore - healthAfter;
         
		 // At most the next two rounds, and never beyond the spell duration.
		 for (int i = 1; i < (int)min(data.duration, 3); ++i)
		 {
			if (dummyArmy->spellInfluence[SPELL_AGE] <= i)
			   dummyArmy->CancelIndividualSpell(SPELL_AGE);

			if (dummyArmy->spellInfluence[SPELL_TOUGHNESS] <= i)
			   dummyArmy->CancelIndividualSpell(SPELL_TOUGHNESS);

			healthBefore = dummyArmy->currentHealth();
      	  
			healthMod = poisonSpellParams[data.mastery].healthMod;
			healthMul = max(dummyArmy->poison_penalty - healthMod, poisonSpellParams[data.mastery].minHealth);
			dummyArmy->poison_penalty = healthMul;
			fullHealth = (double)dummyArmy->origHitPoints * healthMul;

			// Poison, Age & Toughness
			if (dummyArmy->spellInfluence[SPELL_AGE] > i)
			   fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;

			if (dummyArmy->spellInfluence[SPELL_TOUGHNESS] > i)
			   fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;

			health = normalizedStackHealth(fullHealth);

			dummyArmy->sMonInfo.hitPoints = health;

			if (health - 1 < dummyArmy->residualDamage)
			   dummyArmy->residualDamage = health - 1;
      	  
			healthAfter = dummyArmy->currentHealth();
			damage += healthBefore - healthAfter;
		 }

		 delete dummyArmy;

		 int effectiveDamage = (int)(damage * workChance);
		 value = Army->get_loss_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense, Army->can_shoot(),
			effectiveDamage, this->estimate.kills_only);
		 valueParam = effectiveDamage;
	  }
   }

#ifdef NEWSPELLS_DEBUG   
   showSpellInfo(SPELL_POISON, this, Army, Army->AI_target, data, valueParam, value);
#endif

   return value;
}

// Disease
int type_AI_spellcaster::get_disease_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group) || estimate.rounds_left <= 0)
      return 0;

   int value = 0;
   int totalCombatValue = 0;
   
   if (!Army->spellInfluence[SPELL_DISEASE] && isRealArmy(Army->AI_target) &&
       Army->AI_target->numTroops > 0 && !this->win_likely)
   {
	  float workChance = pCombatManager->SpellCastWorkChance(SPELL_DISEASE, this->our_group, Army, false, true, this->is_creature_spell);
	  if (workChance > 0.0f)
	  {
		 int speed = Army->GetSpeed();
		 int targetTime = Army->get_AI_target_time(speed);
		 int rounds_left = this->estimate.rounds_left;

		 if (targetTime <= rounds_left)
		 {
			int spellDuration = data.duration;
   		 
			if (Army->sMonInfo.attributes & CF_DONE)
			   --spellDuration;

			if (spellDuration > 0)
			{
			   int adjSpeed = max(1, speed * o_Spell[SPELL_DISEASE].effect[data.mastery] / 100);

			   if (targetTime == 1)
			   {
				  const int stackCount = max(0, min(21, pCombatManager->stacks_count[this->our_group]));
				  for (int i = 0; i < stackCount; ++i)
				  {
					 army* iArmy = (army*)&pCombatManager->stack[this->our_group][i];
   				  
					 if (iArmy->AI_target == Army &&
						!iArmy->spellInfluence[SPELL_BLIND] &&
						!iArmy->spellInfluence[SPELL_STONE_GAZE] &&
						!iArmy->spellInfluence[SPELL_PARALYZE] &&
						!(iArmy->sMonInfo.attributes & CF_IMMOBILIZED) &&
						iArmy->armyType != eCreatureFirstAidTent &&
						iArmy->armyType != eCreatureAmmoCart &&
						iArmy->GetSpeed() <= speed &&
						iArmy->GetSpeed() > adjSpeed)
					 {
						army* AI_target = iArmy->AI_target;
   					 
						if (isRealArmy(AI_target))
						{
						   const type_AI_combat_parameters* estimate = &this->estimate;
						   int effect = estimate->get_exchange_effect(iArmy, AI_target) + estimate->get_exchange_effect(AI_target, iArmy);
						   if (effect > value)
							  value = effect;
						}
					 }
				  }
			   }

			   if (!Army->can_shoot())
			   {
				  int deltaTargetTime = min(spellDuration, Army->get_AI_target_time(adjSpeed) - targetTime);
   			   
				  if (deltaTargetTime > 0)
				  {
					 totalCombatValue = Army->get_total_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense);
					 int adjTargetTime = min(rounds_left, targetTime + deltaTargetTime);
					 // Weight of speed exchange
					 value += totalCombatValue * (adjTargetTime - targetTime) / rounds_left;
				  }
			   }

			   if (!this->estimate.kills_only)
			   {
				  // Weight of defense penalty
				  totalCombatValue = Army->get_total_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense);
				  value += (int)(totalCombatValue * (1.0 - sqrt(1.0 - diseaseSpellParams[data.mastery].defensePenalty * 0.05)));
   			   
				  // Weight of attack penalty
				  if ((1 << Army->index) & this->enemy_can_attack)
					 value += this->get_attack_skill_value(Army, Army->AI_target, spellDuration,
						min(Army->sMonInfo.attackSkill, diseaseSpellParams[data.mastery].attackPenalty));
			   }

			   value = (int)(value * workChance);
			}
		 }
	  }
   }

#ifdef NEWSPELLS_DEBUG   
   showSpellInfo(SPELL_DISEASE, this, Army, Army->AI_target, data, totalCombatValue, value);
#endif
   
   return value;
}

// Age
int type_AI_spellcaster::get_age_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group))
      return 0;

   int value = 0;
   int valueParam = 0;

   if (!Army->spellInfluence[SPELL_AGE] && !this->win_likely)
   {
	  float workChance = pCombatManager->SpellCastWorkChance(SPELL_AGE, this->our_group, Army, false, true, this->is_creature_spell);
	  if (workChance > 0.0f)
	  {
		 int healthBefore = Army->currentHealth();
            
		 float healthMul = Army->poison_penalty;
		 int ageHealthMod = min(o_Spell[SPELL_AGE].effect[data.mastery], ageSpell[Army->group][Army->index].healthMod);

		 double fullHealth = (double)Army->origHitPoints * healthMul * ageHealthMod / 100.0;

		 // Age & Toughness
		 if (Army->spellInfluence[SPELL_TOUGHNESS])
			fullHealth *= toughnessSpell[Army->group][Army->index].healthMod / 100.0;
       
		 int health = normalizedStackHealth(fullHealth);

		 const int residualDamage = max(0, min(health - 1, Army->residualDamage));
		 int healthAfter = saturatingInt64(
			static_cast<std::int64_t>(health) * Army->numTroops - residualDamage);
		 int effectiveDamage = (int)((healthBefore - healthAfter) * workChance);
         
		 value = Army->get_loss_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense, Army->can_shoot(),
			effectiveDamage, this->estimate.kills_only);
		 valueParam = effectiveDamage;
	  }
   }
   
#ifdef NEWSPELLS_DEBUG   
   showSpellInfo(SPELL_AGE, this, Army, Army->AI_target, data, valueParam, value);
#endif

   return value;
}

// Fear
int type_AI_spellcaster::get_fear_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group) || estimate.rounds_left <= 0)
      return 0;

   int value = 0;
   int totalCombatValue = 0;

   if (!Army->spellInfluence[SPELL_FEAR] &&
	  !Army->spellInfluence[SPELL_BLIND] &&
	  !Army->spellInfluence[SPELL_STONE_GAZE] &&
	  !Army->spellInfluence[SPELL_PARALYZE] &&
	  !this->win_likely)
   {
	  float workChance = pCombatManager->SpellCastWorkChance(SPELL_FEAR, this->our_group, Army, false, true, this->is_creature_spell);
	  if (workChance > 0.0f && (1 << Army->index) & this->enemy_can_attack)
	  {
		 totalCombatValue = Army->get_total_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense);
		 int rounds_left = this->estimate.rounds_left;
		 if (totalCombatValue < this->estimate.awake_enemy_value)
		 {
			double durationRatio =
			   data.duration < rounds_left ? durationRatio = (double)data.duration / rounds_left : 1.0;

			if (Army->sMonInfo.attributes & CF_DONE)
			{
			   durationRatio -= 1.0 / rounds_left;

			   if (durationRatio < 0.0)
				  durationRatio = 0.0;
			}

			totalCombatValue = (int)(totalCombatValue * durationRatio);
		 }
		 else
			totalCombatValue = (int)(totalCombatValue * (0.5 - sqrt(fearSpellParams[data.mastery].retalDamageMod / 400.0)));

		 value = (int)((1.0 + min(data.duration, rounds_left) * 0.25) * totalCombatValue * workChance);
	  }
   }

#ifdef NEWSPELLS_DEBUG   
   showSpellInfo(SPELL_FEAR, this, Army, Army->AI_target, data, totalCombatValue, value);
#endif

   return value;
}

// Death Blow
int type_AI_spellcaster::get_death_blow_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group) || estimate.rounds_left <= 0)
      return 0;

   int value = 0;
   int totalCombatValue = 0;

   if (!Army->spellInfluence[SPELL_DEATH_BLOW] && isRealArmy(Army->AI_target) &&
       Army->AI_target->numTroops > 0 && !this->win_likely)
   {
	  army* targetArmy = new army;
	  targetArmy->copyConstructor(Army);

	  army* enemyArmy = new army;
	  enemyArmy->copyConstructor(Army->AI_target);

	  if (targetArmy->sMonInfo.hitPoints > 0 && !targetArmy->can_shoot() &&
	      targetArmy->get_AI_target_time(targetArmy->GetSpeed()) == 1)
	  {
		 int savedSpellMastery = activeSpellMastery[targetArmy->group][targetArmy->index][SPELL_DEATH_BLOW];

		 int targetArmyHealth = targetArmy->get_total_hit_points();
		 int enemyArmyHealth = enemyArmy->get_total_hit_points();
		 const double deathBlowMultiplier =
			1.0 + o_Spell[SPELL_DEATH_BLOW].effect[data.mastery] / 100.0;
		 int avgDamageBeforeAttack = static_cast<int>(deathBlowMultiplier *
			targetArmy->get_average_damage(enemyArmy, targetArmy->can_shoot(),
			(targetArmy->sMonInfo.hitPoints + targetArmyHealth - 1) /
			targetArmy->sMonInfo.hitPoints) + 0.5);

		 this->estimate.simulate_attack(targetArmy, targetArmyHealth, enemyArmy, enemyArmyHealth, targetArmy->can_shoot());
		 
		 if (targetArmyHealth > 0)
		 {
			int avgDamageAfterAttack = static_cast<int>(deathBlowMultiplier *
			   targetArmy->get_average_damage(enemyArmy, targetArmy->can_shoot(),
			   (targetArmy->sMonInfo.hitPoints + targetArmyHealth - 1) /
			   targetArmy->sMonInfo.hitPoints) + 0.5);

			if (targetArmy->can_shoot() && targetArmy->sMonInfo.attributes & CF_TWO_ATTACKS)
			   avgDamageAfterAttack /= 2;

			double k = 1.0 + (double)avgDamageAfterAttack / max(1, avgDamageBeforeAttack);

			int avgDamage = targetArmy->get_average_damage(enemyArmy, targetArmy->can_shoot(), targetArmy->numTroops);
			int adjAvgDamage = (int)(k * avgDamage);
			targetArmyHealth = targetArmy->get_total_hit_points();

			if (adjAvgDamage > targetArmyHealth)
			{
			   adjAvgDamage = targetArmyHealth;
			   k = (double)targetArmyHealth / max(1, avgDamage);
			}

			if (adjAvgDamage > avgDamage)
			{
			   int rounds_left = this->estimate.rounds_left;
			   double durationRatio = data.duration < rounds_left ? (double)data.duration / rounds_left : 1.0;

			   if (Army->sMonInfo.attributes & CF_DONE)
			   {
				  durationRatio -= 1.0 / rounds_left;

				  if (durationRatio < 0.0)
					 durationRatio = 0.0;
			   }

			   totalCombatValue = targetArmy->get_total_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense);
			   value = (int)((sqrt(k) - 1.0) * totalCombatValue * durationRatio);
			}
		 }
		 
		 activeSpellMastery[targetArmy->group][targetArmy->index][SPELL_DEATH_BLOW] = savedSpellMastery;
	  }

	  delete targetArmy;
	  delete enemyArmy;
   }

#ifdef NEWSPELLS_DEBUG   
   showSpellInfo(SPELL_DEATH_BLOW, this, Army, Army->AI_target, data, totalCombatValue, value);
#endif

   return value;
}

// Drain Life
int type_AI_spellcaster::get_drain_life_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group))
      return 0;

   int value = 0;
   int valueParam = 0;

   if (!Army->spellInfluence[SPELL_DRAIN_LIFE] && !this->win_likely)
   {
	  value = Army->get_total_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense) / 2;
   }

   return value;
}

// Toughness
int type_AI_spellcaster::get_toughness_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group) || Army->origHitPoints <= 0 ||
       Army->numTroops <= 0)
      return 0;

   int value = 0;
   int valueParam = 0;

   if (!Army->spellInfluence[SPELL_TOUGHNESS] && !this->win_likely)
   {
	  int healthBefore = Army->currentHealth();
	  float healthMul = Army->poison_penalty;
	  int toughnessHealthMod = o_Spell[SPELL_TOUGHNESS].effect[data.mastery];
	  double fullHealth = (double)Army->origHitPoints * healthMul * toughnessHealthMod / 100.0;

	  if (Army->spellInfluence[SPELL_AGE])
		 fullHealth *= ageSpell[Army->group][Army->index].healthMod / 100.0;
   
	  int health = normalizedStackHealth(fullHealth);
	  const int residualDamage = max(0, min(health - 1, Army->residualDamage));
	  int healthAfter = saturatingInt64(
		 static_cast<std::int64_t>(health) * Army->numTroops - residualDamage);
	  value = (int)(Army->sMonInfo.baseFightValue *
		 (double)(healthAfter - healthBefore) / Army->origHitPoints);
	  valueParam = healthAfter - healthBefore;
   }

#ifdef NEWSPELLS_DEBUG   
   showSpellInfo(SPELL_TOUGHNESS, this, Army, Army->AI_target, data, valueParam, value);
#endif

   return value;
}

// Behemoth's Claws
int type_AI_spellcaster::get_behemoths_claws_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group) || estimate.rounds_left <= 0)
      return 0;

   int value = 0;
   int totalCombatValue = 0;
   
   if (!Army->spellInfluence[SPELL_BEHEMOTHS_CLAWS] && isRealArmy(Army->AI_target) &&
       Army->AI_target->numTroops > 0 && !this->win_likely)
   {
	  army* targetArmy = new army;
	  targetArmy->copyConstructor(Army);

	  army* enemyArmy = new army;
	  enemyArmy->copyConstructor(Army->AI_target);

	  if (targetArmy->sMonInfo.hitPoints > 0 && !targetArmy->can_shoot() &&
	      targetArmy->get_AI_target_time(targetArmy->GetSpeed()) == 1)
	  {
		 int savedSpellMastery = activeSpellMastery[targetArmy->group][targetArmy->index][SPELL_BEHEMOTHS_CLAWS];

		 ++targetArmy->numSpellInfluences;
		 targetArmy->spellInfluence[SPELL_BEHEMOTHS_CLAWS] = data.duration;
		 activeSpellMastery[targetArmy->group][targetArmy->index][SPELL_BEHEMOTHS_CLAWS] = data.mastery;

		 int targetArmyHealth = targetArmy->get_total_hit_points();
		 int enemyArmyHealth = enemyArmy->get_total_hit_points();
		 int avgDamageBeforeAttack = targetArmy->get_average_damage(enemyArmy, targetArmy->can_shoot(),
			(targetArmy->sMonInfo.hitPoints + targetArmyHealth - 1) / targetArmy->sMonInfo.hitPoints);

		 this->estimate.simulate_attack(targetArmy, targetArmyHealth, enemyArmy, enemyArmyHealth, targetArmy->can_shoot());

		 if (targetArmyHealth > 0)
		 {
			int avgDamageAfterAttack = targetArmy->get_average_damage(enemyArmy, targetArmy->can_shoot(),
			   (targetArmy->sMonInfo.hitPoints + targetArmyHealth - 1) / targetArmy->sMonInfo.hitPoints);

			if (targetArmy->can_shoot() && targetArmy->sMonInfo.attributes & CF_TWO_ATTACKS)
			   avgDamageAfterAttack /= 2;
            
			double k = 1.0 + (double)avgDamageAfterAttack / max(1, avgDamageBeforeAttack);

			int avgDamage = targetArmy->get_average_damage(enemyArmy, targetArmy->can_shoot(), targetArmy->numTroops);
			int adjAvgDamage = (int)(k * avgDamage);
			targetArmyHealth = targetArmy->get_total_hit_points();

			if (adjAvgDamage > targetArmyHealth)
			{
			   adjAvgDamage = targetArmyHealth;
			   k = (double)targetArmyHealth / max(1, avgDamage);
			}
            
			if (adjAvgDamage > avgDamage)
			{
			   int rounds_left = this->estimate.rounds_left;
			   double durationRatio =
				  data.duration < rounds_left ? (double)data.duration / rounds_left : 1.0;

			   if (Army->sMonInfo.attributes & CF_DONE)
			   {
				  durationRatio -= 1.0 / rounds_left;

				  if (durationRatio < 0.0)
					 durationRatio = 0.0;
			   }

			   totalCombatValue = targetArmy->get_total_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense);
			   value = (int)((sqrt(k) - 1.0) * totalCombatValue * durationRatio);
			}
		 }

		 activeSpellMastery[targetArmy->group][targetArmy->index][SPELL_BEHEMOTHS_CLAWS] = savedSpellMastery;
	  }
      
	  delete targetArmy;
	  delete enemyArmy;
   }

#ifdef NEWSPELLS_DEBUG   
   showSpellInfo(SPELL_BEHEMOTHS_CLAWS, this, Army, Army->AI_target, data, totalCombatValue, value);
#endif

   return value;
}

// Hour of Power
int type_AI_spellcaster::get_hour_of_power_value(army* const Army, type_enchant_data data) const
{
   if (!hasValidAiSpellInput(Army, data, our_group))
      return 0;

   int value = 0;
   int valueParam = 0;

   if (!Army->spellInfluence[SPELL_HOUR_OF_POWER] && !this->win_likely)
   {
	  value = Army->get_total_combat_value(this->estimate.lowest_attack, this->estimate.lowest_defense) / 2;
   }

   return value;
}

// Default
int type_AI_spellcaster::unimplemented(army* const Army, type_enchant_data data) const
{
   return 0;
}

int (type_AI_spellcaster::*type_AI_spellcaster::get_enchantment_function(SpellID spell) const)(army* const, type_enchant_data) const
{
   switch (spell)
   {
   case SPELL_ANTI_MAGIC:
	  return &type_AI_spellcaster::get_antimagic_value;
   case SPELL_POISON:
	  return &type_AI_spellcaster::get_poison_value;
   case SPELL_DISEASE:
      return &type_AI_spellcaster::get_disease_value;
   case SPELL_AGE:
      return &type_AI_spellcaster::get_age_value;
   case SPELL_FEAR:
      return &type_AI_spellcaster::get_fear_value;
   case SPELL_DEATH_BLOW:
      return &type_AI_spellcaster::get_death_blow_value;
   case SPELL_DRAIN_LIFE:
      return &type_AI_spellcaster::get_drain_life_value;
   case SPELL_TOUGHNESS:
	  return &type_AI_spellcaster::get_toughness_value;
   case SPELL_BEHEMOTHS_CLAWS:
	  return &type_AI_spellcaster::get_behemoths_claws_value;
   case SPELL_HOUR_OF_POWER:
	  return &type_AI_spellcaster::get_hour_of_power_value;
   default:
	  return &type_AI_spellcaster::unimplemented;
   }
}

int __stdcall get_enchantment_function(HiHook* h, type_AI_spellcaster* caster, SpellID spell)
{
   int (__thiscall type_AI_spellcaster::*value_func)(army* const, type_enchant_data) const = caster->get_enchantment_function(spell);

   if (getExternalSpellSlot(spell))
      return (int&)value_func;

   switch (spell)
   {
   case SPELL_ANTI_MAGIC:
   case SPELL_POISON:
   case SPELL_DISEASE:
   case SPELL_AGE:
   case SPELL_FEAR:
   case SPELL_DEATH_BLOW:
   case SPELL_DRAIN_LIFE:
   case SPELL_TOUGHNESS:
   case SPELL_BEHEMOTHS_CLAWS:
   case SPELL_HOUR_OF_POWER:
	  return (int&)value_func;
   case SPELL_EXPLOSION:
   case SPELL_GOLDEN_TOUCH:
	  // get_damage_spell_value
	  return 0x436BB0;
   default:
	  return CALL_2(int, __thiscall, h->GetDefaultFunc(), caster, spell);
   }
}

int __stdcall consider_spell(LoHook* h, HookContext* c)
{
   enum SpellID spell = (SpellID)c->edi;
   if (!c->ebx || !c->esi)
      return EXEC_DEFAULT;

   int schoolLevel = *(int*)(c->ebx + 4);
   int spellPower = *(int*)(c->ebx + 8);
   type_AI_spellcaster* caster = (type_AI_spellcaster*)c->esi;
   type_spell_choice* spellChoice = (type_spell_choice*)c->ebx;

   ExternalSpellSlot* const external = getExternalSpellSlot(spell);
   NsDataSpell* const data = external && external->active ? nsDataSpell(spell) : 0;
   if (external && !(data && (data->kind == NS_KIND_DAMAGE || data->kind == NS_KIND_AREA_DAMAGE)))
   {
      if (!external->active ||
          (external->descriptor.flags & NEWSPELLS_PROVIDER_HUMAN_ONLY) ||
          !(external->descriptor.capabilities & NEWSPELLS_CAP_COMBAT_AI))
      {
         spellChoice->value = 0;
         spellChoice->cast_now = false;
         c->return_address = 0x43B9BF;
         return NO_EXEC_DEFAULT;
      }

      NewSpellsAiContextV1 context = {};
      context.size = sizeof(context);
      context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
      context.planner = caster;
      context.combatManager = pCombatManager;
      context.casterHero = caster->current_hero;
      context.targetStack = findBattleStackAtHex(spellChoice->target_hex);
      context.spellId = spell;
      context.casterSide = caster->our_group;
      context.targetHex = spellChoice->target_hex;
      context.mastery = schoolLevel;
      context.spellPower = spellPower;
      context.duration = spellChoice->data.duration;
      context.score = spellChoice->value;
      context.castNow = spellChoice->cast_now ? 1 : 0;
      const int32_t result = invokeAiCallback(*external,
         external->descriptor.EvaluateCombatAi, context);
      if (result == NEWSPELLS_PROVIDER_COMMITTED)
      {
         spellChoice->target_hex = context.targetHex;
         spellChoice->value = max(0, context.score);
         spellChoice->cast_now = context.castNow != 0;
      }
      else
      {
         if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*external);
         spellChoice->value = 0;
         spellChoice->cast_now = false;
      }
      c->return_address = 0x43B9BF;
      return NO_EXEC_DEFAULT;
   }
   
   switch (spell)
   {
   case SPELL_DEATH_CLOUD_NEW:
   case SPELL_INCINERATION:
	  c->return_address = 0x43B9D9;
	  return NO_EXEC_DEFAULT;

   case SPELL_SUMMON_FIRE_ELEMENTAL:
   case SPELL_SUMMON_EARTH_ELEMENTAL:
   case SPELL_SUMMON_WATER_ELEMENTAL:
   case SPELL_SUMMON_AIR_ELEMENTAL:
   case SPELL_SUMMON_SPRITE:
   case SPELL_SUMMON_MAGIC_ELEMENTAL:
   case SPELL_SUMMON_FIREBIRD:
	  if (schoolLevel >= eMasteryNone && schoolLevel <= eMasteryExpert &&
	      spellPower > 0 && pCombatManager && !caster->win_likely &&
	      caster->our_group >= ATTACKER && caster->our_group <= DEFENDER &&
	      pCombatManager->AbleToSummonElemental(spell, caster->our_group))
      {
		 spellChoice->cast_now = true;
         
         const int unitsNum = saturatingPositiveDouble(
            getSummoningEffect(spell, schoolLevel) * spellPower, 1);

		 const int k = max(0, caster->estimate.kills_only ? 1000 :
			o_pCreatureInfo[get_elemental_type(spell)].AI_value);
		 spellChoice->value = saturatingInt64(
			static_cast<std::int64_t>(k) * unitsNum);
      }
      break;
   default:
      return EXEC_DEFAULT;
   }
   
   c->return_address = 0x43B9BF;
   return NO_EXEC_DEFAULT;
}

int __stdcall getCancelValue(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->ebx;

   if (hasValidArmyCoordinates(Army) && c->esi >= 0 && c->esi < SPELLS_NUM)
   {
	  int spell = c->esi;
	  *(int*)(c->ebp - 0x28) = activeSpellMastery[Army->group][Army->index][spell];
   }

   return EXEC_DEFAULT;
}
// ===============================================================

// Keep the AI planner consistent with the execution-time Fear restrictions.
// These hooks were added upstream in 1.03 RC2 but omitted by the ERA port.
int __stdcall AI_TreatFearEffect(LoHook* h, HookContext* c)
{
   army* Army = reinterpret_cast<army*>(c->ebx);
   if (isRealArmy(Army) && Army->spellInfluence[SPELL_FEAR])
   {
      c->return_address = 0x43A438;
      return NO_EXEC_DEFAULT;
   }
   return EXEC_DEFAULT;
}

int __stdcall AI_TreatFearEffect2(LoHook* h, HookContext* c)
{
   army* Army = reinterpret_cast<army*>(c->eax - 0x2B0);
   if (isRealArmy(Army) && Army->spellInfluence[SPELL_FEAR])
   {
      c->return_address = 0x43A50D;
      return NO_EXEC_DEFAULT;
   }
   return EXEC_DEFAULT;
}

int __stdcall AI_TreatFearEffect3(LoHook* h, HookContext* c)
{
   army* Army = reinterpret_cast<army*>(c->ebx);
   if (isRealArmy(Army) && Army->spellInfluence[SPELL_FEAR])
   {
      c->return_address = 0x43A67C;
      return NO_EXEC_DEFAULT;
   }
   return EXEC_DEFAULT;
}

// ===============================================================
// ---------------------------- Fear -----------------------------
// ---------------------------------------------------------------
int __stdcall skipMeleeAttackUnderFear(LoHook* h, HookContext* c)
{
   army* Army = pCombatManager ? pCombatManager->GetActiveStack() : 0;
   
   if (Army && Army->spellInfluence[SPELL_FEAR])
   {
      c->return_address = 0x4220B3;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall skipShootingUnderFear(LoHook* h, HookContext* c)
{
   army* Army = pCombatManager ? pCombatManager->GetActiveStack() : 0;
   
   if (Army && Army->spellInfluence[SPELL_FEAR])
   {
      c->return_address = 0x41F263;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall setCursorForFearSpell(HiHook* h, CombatManager* combatMgr, int hex)
{
   int cursorType = CALL_2(int, __thiscall, h->GetDefaultFunc(), combatMgr, hex);

   army* Army = combatMgr ? combatMgr->GetActiveStack() : 0;
      
   if (Army && Army->spellInfluence[SPELL_FEAR] && (cursorType == 3 || cursorType == 15 || cursorType == 7))
      return 0;

   return cursorType;
}
// ===============================================================

// Death Cloud, Explosion, Incineration, Golden Touch
#include "NsDataProvider.h"

int __stdcall castBattleSpell(LoHook* h, HookContext* c)
{
   int targetCell = *(int*)(c->ebp + 0xC);
   int iSpellType = c->edx;
   TSkillMastery mastery = (TSkillMastery)c->esi;
   int power = *(int*)(c->ebp + 0x1C);
   army* Army = (army*)c->edi;

   if (iSpellType >= ORIG_SPELLS_NUM && pCombatManager)
      nsFireBattleCast(iSpellType, pCombatManager->current_side, targetCell, mastery, power, *(int*)(c->ebp + 0x10) != 0);

   ExternalSpellSlot* const external = getExternalSpellSlot(iSpellType);
   if (external)
   {
      PendingExternalCombatTransaction* const pending =
         findPendingExternalCombatTransaction(c->ebp, iSpellType);
      if (!external->active ||
          !(external->descriptor.capabilities & NEWSPELLS_CAP_COMBAT_CAST) ||
          !pCombatManager || mastery < eMasteryNone ||
          mastery > eMasteryExpert)
      {
         cancelExternalCombatTransaction(c, pending);
         return NO_EXEC_DEFAULT;
      }

      const int side = pCombatManager->current_side;
      const int casterKind = *reinterpret_cast<int*>(c->ebp + 0x10);
      hero* const Hero = side >= ATTACKER && side <= DEFENDER
         ? reinterpret_cast<hero*>(pCombatManager->hero[side]) : 0;
      if (external->descriptor.flags & NEWSPELLS_PROVIDER_HUMAN_ONLY)
      {
         _Player_* const player = Hero && o_GameMgr
            ? o_GameMgr->GetPlayer(Hero->playerOwner) : 0;
         if (!player || !player->IsHuman())
         {
            cancelExternalCombatTransaction(c, pending);
            return NO_EXEC_DEFAULT;
         }
      }

      // Both kinds cast through the engine's own switch cases, which end in the epilogue.
      NsDataSpell* const data = nsDataSpell(iSpellType);
      if (data && (data->kind == NS_KIND_DAMAGE || data->kind == NS_KIND_ENCHANTMENT))
      {
         if (pending)
            pending->valid = false;
         if (data->kind == NS_KIND_ENCHANTMENT)
            return EXEC_DEFAULT;
         c->return_address = 0x5A0E2A;
         return NO_EXEC_DEFAULT;
      }

      NewSpellsCombatContextV1 context = {};
      context.size = sizeof(context);
      context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
      context.combatManager = pCombatManager;
      context.casterHero = Hero;
      context.casterStack = pCombatManager->GetActiveStack();
      context.targetStack = isRealArmy(Army) ? Army : findBattleStackAtHex(targetCell);
      context.spellId = iSpellType;
      context.casterSide = side;
      context.targetHex = targetCell;
      context.mastery = mastery;
      context.spellPower = power;
      context.source = casterKind == 0 ? NEWSPELLS_SOURCE_HERO :
         NEWSPELLS_SOURCE_CREATURE;
      const int32_t result = invokeCombatCallback(*external,
         external->descriptor.CastCombat, context);
      if (result == NEWSPELLS_PROVIDER_COMMITTED)
      {
         if (pending)
            pending->valid = false;
         // The normal committed-effect epilogue retains engine mana and
         // once-per-round accounting performed before this dispatch.
         c->return_address = 0x5A2368;
         return NO_EXEC_DEFAULT;
      }

      if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
         failExternalSpellClosed(*external);
      cancelExternalCombatTransaction(c, pending);
      return NO_EXEC_DEFAULT;
   }
   
   switch (iSpellType)
   {
   case SPELL_DEATH_CLOUD_NEW:
   case SPELL_INCINERATION:
	  if (pCombatManager && mastery >= eMasteryNone && mastery <= eMasteryExpert)
	     pCombatManager->AreaEffect(targetCell, iSpellType, mastery, power);
	  break;
   case SPELL_EXPLOSION:
	  if (explosionSpeedReduction && isRealArmy(Army) &&
	      !(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
	  {
		 explosionSpell[Army->group][Army->index].speedPenalty = max(1, Army->sMonInfo.speed - 1) < Army->sMonInfo.speed;
		 Army->sMonInfo.speed = max(1, Army->sMonInfo.speed - 1);
	  }
	  c->return_address = 0x5A0E2A;
	  return NO_EXEC_DEFAULT;
   case SPELL_GOLDEN_TOUCH:
	  if (mastery < eMasteryNone || mastery > eMasteryExpert)
	     break;
	  if (mastery < eMasteryExpert)
	  {
		 c->return_address = 0x5A0E2A;
		 return NO_EXEC_DEFAULT;
	  }
	  else
		 pCombatManager->AreaEffect(targetCell, iSpellType, mastery, power);
	  break;
   default:
	  return EXEC_DEFAULT;
   }

   c->return_address = 0x5A2368;
   return NO_EXEC_DEFAULT;
}

// Both features extend the same indirect spell-dispatch instruction. Keep one
// deterministic NewSpells callback so behavior does not depend on ordering of
// two same-owner LoHooks.
int __stdcall battleSpellDispatcher(LoHook* h, HookContext* c)
{
   int result = castBattleSpell(h, c);
   if (result == EXEC_DEFAULT)
      result = SummonCreatures(h, c);
   if (result != EXEC_DEFAULT)
      return result;

   c->eax = c->edx - SPELL_QUICKSAND;
   c->return_address = (unsigned int)c->eax > (unsigned int)(activeSpellCount - 1 - SPELL_QUICKSAND)
      ? 0x5A2368 : 0x5A0655;
   return NO_EXEC_DEFAULT;
}

int __stdcall spellTableAHelper(LoHook* h, HookContext* c)
{
   int iSpellType = c->ebx;
   TSkillMastery mastery = *(TSkillMastery*)(c->ebp + 8);

   ExternalSpellSlot* const external = getExternalSpellSlot(iSpellType);
   if (external)
   {
      c->edx = external->active ? external->combatTargetDispatch : 4;
      return EXEC_DEFAULT;
   }
   
   switch (iSpellType)
   {
   case SPELL_GOLDEN_TOUCH:
	  o_Spell[SPELL_GOLDEN_TOUCH].type = 0;
	  if (mastery < eMasteryExpert)
	  {
		 o_Spell[SPELL_GOLDEN_TOUCH].flags = SF_BATTLE_SPELL|SF_SINGLE_TARGET|SF_DAMAGE_SPELL|SF_NOT_AT_WAR_MACHINE|SF_AI_DAMAGE_SPELL; //37393;
		 c->edx = 4;
	  }
	  else
	  {
		 o_Spell[SPELL_GOLDEN_TOUCH].flags = SF_BATTLE_SPELL|SF_TARGET_ANYWHERE|SF_DAMAGE_SPELL|SF_NOT_AT_WAR_MACHINE|SF_AI_DAMAGE_SPELL; //37505;
		 c->edx = 5;
	  }
	  break;
   }

   return EXEC_DEFAULT;
}

int __stdcall spellTableFHelper(LoHook* h, HookContext* c)
{
   int iSpellType = c->edi;
   type_spell_choice* spell_choice = *(type_spell_choice**)(c->ebp + 8);
   if (!spell_choice)
      return EXEC_DEFAULT;

   TSkillMastery mastery = spell_choice->data.mastery;
   
   switch (iSpellType)
   {
   case SPELL_GOLDEN_TOUCH:
	  if (mastery < eMasteryExpert)
	  {
		 o_Spell[SPELL_GOLDEN_TOUCH].type = -1;
		 o_Spell[SPELL_GOLDEN_TOUCH].flags = SF_BATTLE_SPELL|SF_SINGLE_TARGET|SF_DAMAGE_SPELL|SF_NOT_AT_WAR_MACHINE|SF_AI_DAMAGE_SPELL; //37393;
		 c->ecx = 9;
	  }
	  else
	  {
		 o_Spell[SPELL_GOLDEN_TOUCH].type = 0;
		 o_Spell[SPELL_GOLDEN_TOUCH].flags = SF_BATTLE_SPELL|SF_TARGET_ANYWHERE|SF_DAMAGE_SPELL|SF_NOT_AT_WAR_MACHINE|SF_AI_DAMAGE_SPELL; //37505;
		 c->ecx = 2;
	  }
	  break;
   }

   return EXEC_DEFAULT;
}

int __stdcall considerSpellDispatcher(LoHook* h, HookContext* c)
{
   spellTableFHelper(h, c);
   return consider_spell(h, c);
}

int __stdcall showAreaAnim(LoHook* h, HookContext* c)
{
   SpellID spell = *(SpellID*)(c->ebp + 0xC);

   if (spell == SPELL_GOLDEN_TOUCH)
   {
	  *(int*)(c->ebp - 0x18) = c->ebx;
	  *(unsigned int*)(c->ebp - 0x14) = static_cast<unsigned int>(c->eax) * 8u;

	  c->return_address = 0x5A4CCD;
	  return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall goldenTouchAreaAnim(LoHook* h, HookContext* c)
{
   SpellID spell = *(SpellID*)(c->ebp + 0xC);

   if (spell == SPELL_GOLDEN_TOUCH)
   {
	  for (int side = ATTACKER; side <= DEFENDER; ++side)
	  {
		 for (int i = 0; i < 20; ++i)
		 {
			army* Army = (army*)&pCombatManager->stack[side][i];
			
			if (Army && massSpellTarget[side][i])
			{
			   float workChance = pCombatManager->SpellCastWorkChance(SPELL_GOLDEN_TOUCH, pCombatManager->current_side, Army);

			   //debugStr("%s: %d (%f)", Army->sMonInfo.name_plural, Army->count_current, workChance);
			   
			   if (!workChance)
				  massSpellTarget[side][i] = false;
			}
		 }
	  }
	  
	  pCombatManager->ShowMassSpell(massSpellTarget, o_Spell[SPELL_GOLDEN_TOUCH].animation_ix, false);
   }

   return EXEC_DEFAULT;
}

int getGoldenTouchEffect(hero* const Hero, army* const Army, const int killed_creatures)
{
   if (!Hero || !isRealArmy(Army) || killed_creatures <= 0)
      return 0;

   double goldenTouchEffect[] = {0.10, 0.10, 0.15, 0.20};
   int mastery = Hero->SSLevel[Hero->getBestMagicSchoolForSpell(SPELL_GOLDEN_TOUCH)];
   if (mastery < eMasteryNone || mastery > eMasteryExpert)
      mastery = eMasteryNone;

   const double gold = killed_creatures * static_cast<double>(max(Army->sMonInfo.cost[GOLD], 0)) *
      goldenTouchEffect[mastery];
   if (gold >= 2147483647.0)
      return 0x7FFFFFFF;
   return max(1, static_cast<int>(gold));
}

int saturatingAddNonNegative(int left, int right)
{
   const __int64 sum = static_cast<__int64>(max(left, 0)) + max(right, 0);
   return sum >= 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int>(sum);
}

int __stdcall goldenTouchAddGoldSingleTarget(LoHook* h, HookContext* c)
{
   SpellID spell = *(SpellID*)(c->ebp + 8);
   if (!pCombatManager || pCombatManager->current_side < ATTACKER ||
       pCombatManager->current_side > DEFENDER)
      return EXEC_DEFAULT;

   const int side = pCombatManager->current_side;
   hero* Hero = (hero*)pCombatManager->hero[side];
   
   if (Hero && spell == SPELL_GOLDEN_TOUCH)
   {
	  army* Army = (army*)c->edi;
	  int killed_creatures = c->eax;
	  
	  if (killed_creatures > 0 && Hero->playerOwner >= 0 && Hero->playerOwner < 8)
	  {
		 int goldTransmuted = getGoldenTouchEffect(Hero, Army, killed_creatures);
		 if (goldTransmuted > 0)
		 {
			goldenTouchSpell[side].goldForCast = goldTransmuted;
			goldenTouchSpell[side].goldForBattle =
			   saturatingAddNonNegative(goldenTouchSpell[side].goldForBattle, goldTransmuted);
			_Player_* player = o_GameMgr ? o_GameMgr->GetPlayer(Hero->playerOwner) : 0;
			if (player)
			   player->resourses.gold = saturatingAddNonNegative(player->resourses.gold, goldTransmuted);
		 }
	  }
   }

   return EXEC_DEFAULT;
}

int __stdcall goldenTouchAddGold(LoHook* h, HookContext* c)
{
   SpellID spell = *(SpellID*)(c->ebp + 0xC);
   if (!pCombatManager || pCombatManager->current_side < ATTACKER ||
       pCombatManager->current_side > DEFENDER)
      return EXEC_DEFAULT;

   const int side = pCombatManager->current_side;
   hero* Hero = (hero*)pCombatManager->hero[side];
   
   if (Hero && spell == SPELL_GOLDEN_TOUCH)
   {
	  army* Army = (army*)c->edi;
	  int killed_creatures = c->eax;
	  
	  if (killed_creatures > 0 && Hero->playerOwner >= 0 && Hero->playerOwner < 8)
	  {
		 int goldTransmuted = getGoldenTouchEffect(Hero, Army, killed_creatures);
		 if (goldTransmuted > 0)
		 {
			goldenTouchSpell[side].goldForCast =
			   saturatingAddNonNegative(goldenTouchSpell[side].goldForCast, goldTransmuted);
			goldenTouchSpell[side].goldForBattle =
			   saturatingAddNonNegative(goldenTouchSpell[side].goldForBattle, goldTransmuted);
			_Player_* player = o_GameMgr ? o_GameMgr->GetPlayer(Hero->playerOwner) : 0;
			if (player)
			   player->resourses.gold = saturatingAddNonNegative(player->resourses.gold, goldTransmuted);
		 }
	  }
   }

   return EXEC_DEFAULT;
}

char goldenTouchTxtBuffer[1024];
int __stdcall appendCombatLogInfo(LoHook* h, HookContext* c)
{
   if (!pCombatManager || pCombatManager->current_side < ATTACKER ||
       pCombatManager->current_side > DEFENDER)
      return EXEC_DEFAULT;

   const int side = pCombatManager->current_side;
   if (goldenTouchSpell[side].goldForCast)
   {
	  char* msg = (char*)c->eax;
	  sprintf_s(goldenTouchTxtBuffer, sizeof(goldenTouchTxtBuffer), "%s %d %s",
		 msg ? msg : "", goldenTouchSpell[side].goldForCast,
		 goldenTouchCombatLogMessage ? goldenTouchCombatLogMessage : emptyLocalizedText);
	  goldenTouchSpell[side].goldForCast = 0;
	  c->eax = (int)goldenTouchTxtBuffer;
   }

   return EXEC_DEFAULT;
}

int __stdcall notifyPlayerAfterBattle(LoHook* h, HookContext* c)
{
   if (!o_GameMgr || !pCombatManager)
      return EXEC_DEFAULT;

   _Player_* const currentPlayer = o_GameMgr->GetMe();
   if (!currentPlayer || !currentPlayer->IsHuman())
      return EXEC_DEFAULT;

   for (int i = ATTACKER; i <= DEFENDER; ++i)
   {
	  if (goldenTouchSpell[i].goldForBattle && pCombatManager->hero[i] &&
		  pCombatManager->hero[i]->owner_id == o_GameMgr->GetMeID())
		 CALL_12(void, __fastcall, 0x4F6C00,
			goldenTouchWinBattleMessage ? goldenTouchWinBattleMessage : emptyLocalizedText,
			MBX_OK, -1, -1, 6, goldenTouchSpell[i].goldForBattle, -1, 0, -1, 0, -1, 0);
   }

   return EXEC_DEFAULT;
}

int __stdcall applyIncinerationSpecialFeature(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->edi;
   int damage = c->eax;

   if (!isRealArmy(Army) || damage < 0)
      return EXEC_DEFAULT;

   *(int*)(c->ebp - 0x24) = damage;
   int numDestroyed = Army->Damage(damage);
   c->eax = numDestroyed;
   
   if (incinerationSpecialFeature)
   {
	  int spell = c->esi;
	  if (spell == SPELL_INCINERATION)
	  {
		 Army->origNumTroops = max(Army->origNumTroops - max(numDestroyed, 0), 0);
		 if (Army->numTroops <= 0)
			Army->sMonInfo.attributes |= CF_SACRIFICED;
	  }
   }

   c->return_address = 0x5A4DF2;
   return NO_EXEC_DEFAULT;
}

/*int army::get_resurrection_size(army* const target) const
{
   int origNum = incinerationSpecialFeature ? origNumTroops[target->group][target->index] : target->origNumTroops;
   
   int destroyedNumTroops = origNum - target->numTroops;
   int unitsToResurrect;

   if (this->armyType == eCreatureArchangel)
	  unitsToResurrect = 100 * this->numTroops / target->sMonInfo.hitPoints;
   else
   {
	  int hitPointsToResurrect = min(50 * this->numTroops, destroyedNumTroops * target->sMonInfo.hitPoints);
	  unitsToResurrect = hitPointsToResurrect / o_pCreatureInfo[eCreatureDemon].hit_points;
   }

   return min(destroyedNumTroops, unitsToResurrect);
}

int __fastcall get_resurection_size(army* const Army, int unused_edx, army* const target)
{
   return Army->get_resurrection_size(target);
}*/

int __stdcall get_area_effect_new(LoHook* h, HookContext* c)
{
   int spell = c->edi;

   switch (spell)
   {
   case SPELL_FROST_RING:
   case SPELL_FIREBALL:
   case SPELL_INFERNO:
   case SPELL_METEOR_SHOWER:
   case SPELL_DEATH_CLOUD_NEW:
   case SPELL_INCINERATION:
   case SPELL_GOLDEN_TOUCH:
	  c->return_address = 0x41FC03;
	  return NO_EXEC_DEFAULT;
   default:
	  return EXEC_DEFAULT;
   }
}

// Death Blow
int __stdcall DeathBlow(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;
   if (!pCombatManager || !isRealArmy(Army))
      return EXEC_DEFAULT;

   int schoolLevel = activeSpellMastery[Army->group][Army->index][SPELL_DEATH_BLOW];
   if (schoolLevel < eMasteryNone || schoolLevel > eMasteryExpert)
      return EXEC_DEFAULT;
   
   if (Army->spellInfluence[SPELL_DEATH_BLOW] && pCombatManager->GetAction() == 6 &&
	  Randint(1, 100) <= o_Spell[SPELL_DEATH_BLOW].effect[schoolLevel])
   {
	  bool virtualDamage = (c->ebx & 0xFF) != 0;
      c->return_address = virtualDamage ? 0x44388E : 0x4436E7;
      return NO_EXEC_DEFAULT;
   }
   
   return EXEC_DEFAULT;
}

// Drain Life
bool usesRawCreatureIdAttackAbilityDispatcher()
{
   // Amethyst extends the native after-attack ability table to all creature
   // IDs. It changes "add eax, -63" to "add eax, 0", removes the stock range
   // check, and redirects the byte-table operand at 0x440916. New Spells is
   // normally loaded first, so its 0x440903 LoHook trampoline has already
   // captured the old subtraction. Detect the complete live contract before
   // bypassing that relocated code.
   static const unsigned char rawCreatureIdDispatcherSignature[] =
   {
      0x00,                                                       // 0x440908: add eax, 0
      0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,   // removed cmp/ja
      0x33, 0xC9, 0x8A, 0x88                                  // xor ecx; mov cl, [...]
   };

   const int attackAbilityTable = *reinterpret_cast<const int*>(0x440916);
   return memcmp(reinterpret_cast<const void*>(0x440908),
                 rawCreatureIdDispatcherSignature,
                 sizeof(rawCreatureIdDispatcherSignature)) == 0 &&
          attackAbilityTable != 0 && attackAbilityTable != 0x4412D8;
}

int __stdcall DrainLife(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;
   
   if (isRealArmy(Army) && Army->spellInfluence[SPELL_DRAIN_LIFE])
   {
      c->return_address = 0x440921;
      return NO_EXEC_DEFAULT;
   }

   if (Army && usesRawCreatureIdAttackAbilityDispatcher())
   {
      // The extended table is indexed by the raw creature ID. Do not execute
      // this LoHook's relocated stock "add eax, -63" instruction.
      c->eax = Army->armyType;
      c->return_address = 0x440909;
      return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

int __stdcall calcDrainLifeHeal(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;

   if (isRealArmy(Army) && Army->spellInfluence[SPELL_DRAIN_LIFE])
   {
      const int mastery = activeSpellMastery[Army->group][Army->index][SPELL_DRAIN_LIFE];
      if (mastery >= eMasteryNone && mastery <= eMasteryExpert)
         c->edx = (int)(c->edx * o_Spell[SPELL_DRAIN_LIFE].effect[mastery] / 100.0);
   }

   return EXEC_DEFAULT;
}

// ===============================================================
// --------------------- Adventure Spells ------------------------
// ---------------------------------------------------------------
bool eyeOfTheMagiCast = false;

int __stdcall eyeOfTheMagi(LoHook* h, HookContext* c)
{
   if (eyeOfTheMagiCast)
   {
	  // This relocated hook replaces `push eax; call AdvMgr_GetItemAtCoords`.
	  // Preserve that call's side effects before taking the Eye-specific branch.
	  if (c->ecx)
		 c->eax = CALL_2(int, __thiscall, 0x412B30,
			reinterpret_cast<AdventureManager*>(c->ecx), c->eax);
	  c->return_address = 0x4915C0;
	  return NO_EXEC_DEFAULT;
   }

   return EXEC_DEFAULT;
}

void __stdcall setMouseCursor(HiHook* h, MouseManager* mouseManager, int new_frame, MouseManager::EPointerSet new_set)
{
   if (eyeOfTheMagiCast && new_frame == 41 && new_set == MouseManager::ADVENTURE_SET)
   {
	  new_frame = 0;
	  new_set = MouseManager::SPELL_SET;
   }

   CALL_3(void, __thiscall, h->GetDefaultFunc(), mouseManager, new_frame, new_set);
}

int mobilitySpellParams[] = {1, 2, 3, 4};
int eyeOfTheMagiSpellParams[] = {1, 2, 3, 4};

void __stdcall evaluateExternalAdventureSpellsForAi(HiHook* h, hero* Hero,
                                                     bool isLastHero,
                                                     bool* exploreMode)
{
   _Player_* const player = hasValidHeroId(Hero) && o_GameMgr
      ? o_GameMgr->GetPlayer(Hero->playerOwner) : 0;
   if (player && !player->IsHuman() && pGame)
   {
      ExternalSpellSlot* selected = 0;
      NewSpellsAiContextV1 selectedEvaluation = {};
      int selectedManaCost = 0;
      int selectedTerrain = 0;

      for (int spellId = NEWSPELLS_EXTERNAL_SPELL_FIRST_ID;
           spellId <= NEWSPELLS_EXTERNAL_SPELL_LAST_ID; ++spellId)
      {
         ExternalSpellSlot* const slot = getExternalSpellSlot(spellId);
         if (!slot || !slot->active ||
             (slot->descriptor.flags & NEWSPELLS_PROVIDER_HUMAN_ONLY) ||
             (slot->descriptor.capabilities &
              (NEWSPELLS_CAP_ADVENTURE_CAST | NEWSPELLS_CAP_ADVENTURE_AI)) !=
             (NEWSPELLS_CAP_ADVENTURE_CAST | NEWSPELLS_CAP_ADVENTURE_AI) ||
             !isDefinedHeroSpell(spellId) || !heroAvailableSpell(Hero, spellId) ||
             pGame->SpellDisabled(static_cast<SpellID>(spellId)))
            continue;

         const int terrain = Hero->get_special_terrain();
         const int mastery = Hero->get_spell_level(
            static_cast<SpellID>(spellId), terrain);
         if (mastery < eMasteryNone || mastery > eMasteryExpert)
            continue;
         const int manaCost = Hero->GetManaCost(spellId, 0, terrain);
         if (manaCost < 0 || manaCost > Hero->mana)
            continue;

         NewSpellsAiContextV1 evaluation = {};
         evaluation.size = sizeof(evaluation);
         evaluation.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         evaluation.planner = pAdventureManager;
         evaluation.casterHero = Hero;
         evaluation.spellId = spellId;
         evaluation.casterSide = Hero->playerOwner;
         evaluation.targetHex = ID_NONE;
         evaluation.mastery = mastery;
         evaluation.spellPower = Hero->stats[2];
         const int32_t result = invokeAiCallback(*slot,
            slot->descriptor.EvaluateAdventureAi, evaluation);
         if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
         {
            failExternalSpellClosed(*slot);
            continue;
         }
         if (result != NEWSPELLS_PROVIDER_COMMITTED || !evaluation.castNow ||
             evaluation.score <= 0)
            continue;

         NewSpellsAdventureContextV1 candidate = {};
         candidate.size = sizeof(candidate);
         candidate.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         candidate.hero = Hero;
         candidate.adventureManager = pAdventureManager;
         candidate.game = pGame;
         candidate.spellId = spellId;
         candidate.mastery = mastery;
         candidate.specialTerrain = terrain;
         candidate.isHuman = 0;
         candidate.target = evaluation.targetHex;
         int32_t validation = NEWSPELLS_PROVIDER_COMMITTED;
         if (slot->descriptor.ValidateAdventure)
            validation = invokeAdventureCallback(*slot,
               slot->descriptor.ValidateAdventure, candidate);
         if (validation == NEWSPELLS_PROVIDER_UNSUPPORTED)
         {
            failExternalSpellClosed(*slot);
            continue;
         }
         if (validation != NEWSPELLS_PROVIDER_COMMITTED ||
             (selected && evaluation.score <= selectedEvaluation.score))
            continue;

         evaluation.targetHex = candidate.target;
         selected = slot;
         selectedEvaluation = evaluation;
         selectedManaCost = manaCost;
         selectedTerrain = terrain;
      }

      if (selected)
      {
         NewSpellsAdventureContextV1 context = {};
         context.size = sizeof(context);
         context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         context.hero = Hero;
         context.adventureManager = pAdventureManager;
         context.game = pGame;
         context.spellId = selected->descriptor.spellId;
         context.mastery = selectedEvaluation.mastery;
         context.specialTerrain = selectedTerrain;
         context.isHuman = 0;
         context.target = selectedEvaluation.targetHex;

         const int32_t result = invokeAdventureCallback(*selected,
            selected->descriptor.CastAdventure, context);
         if (result == NEWSPELLS_PROVIDER_COMMITTED)
         {
            Hero->UseSpell(selectedManaCost);
            _Spell_& spell = o_Spell[selected->descriptor.spellId];
            if (spell.wav_name && *spell.wav_name)
               playSound(spell.wav_name);
         }
         else if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*selected);
      }
   }

   CALL_3(void, __fastcall, h->GetDefaultFunc(), Hero, isLastHero,
      exploreMode);
}

int __stdcall castAdventureSpell(LoHook* h, HookContext* c)
{
   SpellID spell = *(SpellID*)(c->ebp + 8);
   hero* Hero = (hero*)c->esi;
   ExternalSpellSlot* const external = getExternalSpellSlot(spell);
   const bool externalAdventure = external && external->active &&
      (external->descriptor.capabilities & NEWSPELLS_CAP_ADVENTURE_CAST);

   if (external && !externalAdventure)
   {
      c->return_address = 0x41C67B;
      return NO_EXEC_DEFAULT;
   }

   if (spell != SPELL_MOBILITY && spell != SPELL_EYE_OF_THE_MAGI &&
       !externalAdventure)
      return EXEC_DEFAULT;

   // These spells are interactive UI actions. Never enter their modal
   // paths from AI/headless processing.
   const bool activeHumanHero = hasValidHeroId(Hero) && o_ActivePlayer &&
      o_ActivePlayer->IsHuman() && Hero->playerOwner == o_ActivePlayer->id;
   if ((!externalAdventure && !activeHumanHero) ||
       (externalAdventure &&
        (external->descriptor.flags & NEWSPELLS_PROVIDER_HUMAN_ONLY) &&
        !activeHumanHero) || !hasValidHeroId(Hero))
   {
      c->return_address = 0x41C67B;
      return NO_EXEC_DEFAULT;
   }

   int special_terrain = Hero->get_special_terrain();
   int schoolLevel = Hero->get_spell_level(spell, special_terrain);
   const int externalManaCost = externalAdventure
      ? Hero->GetManaCost(spell, 0, special_terrain) : 0;

   if (schoolLevel < eMasteryNone || schoolLevel > eMasteryExpert ||
       (externalAdventure &&
        (externalManaCost < 0 || externalManaCost > Hero->mana)))
   {
      c->return_address = 0x41C67B;
      return NO_EXEC_DEFAULT;
   }

   if (spell >= ORIG_SPELLS_NUM)
      nsFireAdventureCast(Hero, spell, schoolLevel);

   if (externalAdventure)
   {
      NewSpellsAdventureContextV1 context = {};
      context.size = sizeof(context);
      context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
      context.hero = Hero;
      context.adventureManager = pAdventureManager;
      context.game = pGame;
      context.spellId = spell;
      context.mastery = schoolLevel;
      context.specialTerrain = special_terrain;
      context.isHuman = activeHumanHero ? 1 : 0;
      context.target = ID_NONE;

      if (external->descriptor.ValidateAdventure)
      {
         const int32_t validation = invokeAdventureCallback(*external,
            external->descriptor.ValidateAdventure, context);
         if (validation == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*external);
         if (validation != NEWSPELLS_PROVIDER_COMMITTED)
         {
            c->return_address = 0x41C67B;
            return NO_EXEC_DEFAULT;
         }
      }

      const int32_t result = invokeAdventureCallback(*external,
         external->descriptor.CastAdventure, context);
      if (result == NEWSPELLS_PROVIDER_COMMITTED)
      {
         Hero->UseSpell(externalManaCost);
         if (o_Spell[spell].wav_name && *o_Spell[spell].wav_name)
            playSound(o_Spell[spell].wav_name);
      }
      else if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
         failExternalSpellClosed(*external);

      c->return_address = 0x41C67B;
      return NO_EXEC_DEFAULT;
   }

   switch (spell)
   {
   case SPELL_MOBILITY:
	  if (heroAdvInfoEx[Hero->id].mobilityCastCount < mobilitySpellParams[schoolLevel])
	  {
		 if (mobilityRequiresBattle && !heroAdvInfoEx[Hero->id].mobilityWonBattle)
		 {
			sprintf(o_TextBuffer, mobilityWinBattleMessage, Hero->name);
			b_MsgBox(o_TextBuffer, MBX_OK);
		 }
		 else
		 {
			++heroAdvInfoEx[Hero->id].mobilityCastCount;
			heroAdvInfoEx[Hero->id].mobilityWonBattle = false;
			Hero->UseSpell(Hero->GetManaCost(SPELL_MOBILITY, NULL, special_terrain));
			Hero->currMobility += o_Spell[SPELL_MOBILITY].effect[schoolLevel];
			Hero->maxMobility += o_Spell[SPELL_MOBILITY].effect[schoolLevel];
			pAdventureManager->ShowRoute(0, 0, 1);
			pAdventureManager->FullUpdate(true);
			playSound(o_Spell[SPELL_MOBILITY].wav_name);
		 }
	  }
	  else
	  {
		 sprintf(o_TextBuffer, o_GENRLTXT_TXT->GetString(339), Hero->name);
		 b_MsgBox(o_TextBuffer, MBX_OK);
	  }
	  break;

   case SPELL_EYE_OF_THE_MAGI:
	  if (heroAdvInfoEx[Hero->id].eyeOfTheMagiCastCount < eyeOfTheMagiSpellParams[schoolLevel])
	  {
		 HeroWindow* heroWindow = new HeroWindow();
		 CALL_1(void, __thiscall, 0x41D326 + 5 + *(int*)0x41D327, heroWindow);
		 eyeOfTheMagiCast = true;
		 heroWindow->DoModal(false);
		 eyeOfTheMagiCast = false;
		 CALL_1(void, __thiscall, 0x41D346 + 5 + *(int*)0x41D347, heroWindow);
		 delete heroWindow;

		 type_point mapPoint;
		 pAdventureManager->get_mouse_map_point(mapPoint);

		 if (mapPoint.is_valid() && pWindowManager->Field<int>(0x38))
		 {
			int x, y;
			pGame->SetVisibility(mapPoint.x, mapPoint.y, mapPoint.z, o_ActivePlayer->id, o_Spell[SPELL_EYE_OF_THE_MAGI].effect[schoolLevel], 0);
			pAdventureManager->Field<int>(0xEC) = -1;
			pMouseManager->MouseCoords(x, y);
			pAdventureManager->ProcessHover(x, y);
		 }
		 else
		 {
			sprintf(o_TextBuffer, o_GENRLTXT_TXT->GetString(732));
			b_MsgBox(o_TextBuffer, MBX_OK);
			break;
		 }

		 ++heroAdvInfoEx[Hero->id].eyeOfTheMagiCastCount;
		 Hero->UseSpell(Hero->GetManaCost(SPELL_EYE_OF_THE_MAGI, NULL, special_terrain));
		 pAdventureManager->FullUpdate(true);
		 playSound(o_Spell[SPELL_EYE_OF_THE_MAGI].wav_name);
	  }
	  else
	  {
		 sprintf(o_TextBuffer, o_GENRLTXT_TXT->GetString(339), Hero->name);
		 b_MsgBox(o_TextBuffer, MBX_OK);
	  }
	  break;

   default:
	  return EXEC_DEFAULT;
   }
   
   c->return_address = 0x41C67B;
   return NO_EXEC_DEFAULT;
}

void loadFromGameFile(const int addr, void* buffer, const int size)
{
   CALL_3(void, __thiscall, *(int*)(*(int*)addr + 4), addr, buffer, size);
}

void saveToGameFile(const int addr, void* buffer, const int size)
{
   CALL_3(void, __thiscall, *(int*)(*(int*)addr + 8), addr, buffer, size);
}

int __stdcall loadHeroAdvInfoEx(LoHook* h, HookContext* c)
{
   hero* Hero = (hero*)c->edi;

   if (hasValidHeroId(Hero))
   {
	  loadFromGameFile(c->esi, &heroAdvInfoEx[Hero->id].mobilityCastCount, sizeof(heroAdvInfoEx[Hero->id].mobilityCastCount));
	  loadFromGameFile(c->esi, &heroAdvInfoEx[Hero->id].mobilityWonBattle, sizeof(heroAdvInfoEx[Hero->id].mobilityWonBattle));
	  loadFromGameFile(c->esi, &heroAdvInfoEx[Hero->id].eyeOfTheMagiCastCount, sizeof(heroAdvInfoEx[Hero->id].eyeOfTheMagiCastCount));
   }

   return EXEC_DEFAULT;
}

int __stdcall saveHeroAdvInfoEx(LoHook* h, HookContext* c)
{
   hero* Hero = (hero*)c->edi;
   
   if (hasValidHeroId(Hero))
   {
	  saveToGameFile(c->esi, &heroAdvInfoEx[Hero->id].mobilityCastCount, sizeof(heroAdvInfoEx[Hero->id].mobilityCastCount));
	  saveToGameFile(c->esi, &heroAdvInfoEx[Hero->id].mobilityWonBattle, sizeof(heroAdvInfoEx[Hero->id].mobilityWonBattle));
	  saveToGameFile(c->esi, &heroAdvInfoEx[Hero->id].eyeOfTheMagiCastCount, sizeof(heroAdvInfoEx[Hero->id].eyeOfTheMagiCastCount));
   }

   return EXEC_DEFAULT;
}
// ===============================================================

bool artifactGrantsExtendedSpell(const int artifactId, const int spellId)
{
   if (spellId < DEFAULT_SPELLS_NUM || !isDefinedHeroSpell(spellId))
      return false;

   switch (artifactId)
   {
   case eArtifactTomeOfFireMagic:
      return (o_Spell[spellId].school_flags & SSF_FIRE) != 0;
   case eArtifactTomeOfAirMagic:
      return (o_Spell[spellId].school_flags & SSF_AIR) != 0;
   case eArtifactTomeOfWaterMagic:
      return (o_Spell[spellId].school_flags & SSF_WATER) != 0;
   case eArtifactTomeOfEarthMagic:
      return (o_Spell[spellId].school_flags & SSF_EARTH) != 0;
   case eArtifactSpellbindersHat:
      return o_Spell[spellId].level == 5;
   default:
      return false;
   }
}

void addArtifactSpells(hero* const Hero, const int artifactId)
{
   _SpellBitset_ spells = {};
   CALL_2(int, __fastcall, 0x4D95C0, &spells, artifactId);

   for (int spellId = 0; spellId < activeSpellCount; ++spellId)
      if ((spells[spellId] || artifactGrantsExtendedSpell(artifactId, spellId)) &&
          isDefinedHeroSpell(spellId))
         heroAvailableSpell(Hero, spellId) = 1;
}

// Emerald replaces the combo table with 124-byte records (index + 30 words), the layout is
// recognized by the index fields of records 1 and 2.
struct _ComboArtInfoWide_ { int index; unsigned int parts[30]; };
bool hasComboPart(int comboArtIndex, int artifact)
{
   const int* table = (const int*)o_ComboArtInfo;
   if (table[6] != 1 && table[31] == 1 && table[62] == 2)
      return ((*(_ComboArtInfoWide_**)0x660B6C)[comboArtIndex].parts[artifact >> 5] >> (artifact & 31) & 1) != 0;
   return o_ComboArtInfo[comboArtIndex].HasPart(artifact);
}

void hero::UpdateSpellsFromArtifacts()
{
   for (int iSpell = ORIG_SPELLS_NUM; iSpell < SPELLS_MAX; ++iSpell)
	  heroAvailableSpell(this, iSpell) = heroInSpellbook(this, iSpell);

   for (int iSlot = 0; iSlot < 19; ++iSlot)
   {
	  const int artifactId = this->equipped[iSlot].type;
	  if (artifactId >= 0 && artifactId < ARTIFACTS_NUM)
      {
         if (artifactId == eArtifactSpellScroll)
         {
			const int spellId = this->equipped[iSlot].spell;
			if (isDefinedHeroSpell(spellId))
			   heroAvailableSpell(this, spellId) = 1;
         }
         else if (o_ArtInfo[artifactId].new_spell)
         {
            addArtifactSpells(this, artifactId);

            int comboArtIndex = o_ArtInfo[artifactId].supercomposite;
			
			if (o_ComboArtInfo && comboArtIndex >= 0 &&
				comboArtIndex < COMBINATION_ARTIFACTS_NUM)
            {
               for (int iArt = 0; iArt < ARTIFACTS_NUM; ++iArt)
               {
                  if (hasComboPart(comboArtIndex, iArt) && o_ArtInfo[iArt].new_spell)
                     addArtifactSpells(this, iArt);
               }
            }
         }
      }
   }
}

// The exe body serves ids below 70, with the hooks other plugins keep inside it.
void __stdcall UpdateSpellsFromArtifacts(HiHook* h, hero* Hero)
{
   CALL_1(void, __thiscall, h->GetDefaultFunc(), Hero);
   if (hasValidHeroId(Hero))
      Hero->UpdateSpellsFromArtifacts();
}

// Spell scroll in the exe body: "mov [ebx+eax+430h], dl"
int __stdcall artifactScrollSpell(LoHook* h, HookContext* c)
{
   if ((unsigned int)c->eax < SPELLS_MAX)
      heroAvailableSpell((hero*)c->ebx, c->eax) = 1;
   c->return_address = 0x4D9894;
   return NO_EXEC_DEFAULT;
}

int army::GetSpeed()
{
   int speed = this->sMonInfo.speed;

   if (this->spellInfluence[SPELL_SLOW] || this->spellInfluence[SPELL_DISEASE] || this->spellInfluence[SPELL_FEAR])
	  speed = this->sMonInfo.attributes & CF_SIEGE_WEAPON ? 0 : max((int)(speed * this->slowPenalty), 1);

   const int percent = nsDataSpeedPercent(this);
   if (percent != 100 && !(this->sMonInfo.attributes & CF_SIEGE_WEAPON))
      speed = max(speed * percent / 100, 1);

   return speed;
}

int __stdcall GetSpeed(HiHook* h, army* Army)
{
   int speed = CALL_1(int, __thiscall, h->GetDefaultFunc(), Army);
   if (!Army)
      return speed;

   if (!Army->spellInfluence[SPELL_SLOW] && (Army->spellInfluence[SPELL_DISEASE] || Army->spellInfluence[SPELL_FEAR]))
      speed = Army->sMonInfo.attributes & CF_SIEGE_WEAPON ? 0 : max((int)(speed * Army->slowPenalty), 1);

   const int percent = nsDataSpeedPercent(Army);
   if (percent != 100 && !(Army->sMonInfo.attributes & CF_SIEGE_WEAPON))
      speed = max(speed * percent / 100, 1);

   return speed;
}

int __stdcall setSpellInfluence(LoHook* h, HookContext* c)
{
   army* Army = (army*)c->esi;	 
   int spell = c->ebx;
   int spellDuration = c->eax;
   int spellMastery = *(int*)(c->ebp + 0x10);

   if (!hasValidArmyCoordinates(Army) || spell < 0 || spell >= SPELLS_NUM ||
       spellDuration <= 0 ||
       spellMastery < eMasteryNone || spellMastery > eMasteryExpert)
   {
      // The hook is before the game's first spell-array access and before EDI
      // is pushed. Leave the stack untouched and return through the normal
      // epilogue instead of allowing corrupt coordinates to index sidecars.
      c->return_address = 0x4450C2;
      return NO_EXEC_DEFAULT;
   }

   ExternalSpellSlot* const external = getExternalSpellSlot(spell);
   if (external && (!external->active ||
       !(external->descriptor.capabilities & NEWSPELLS_CAP_STATUS_APPLY)))
   {
      c->return_address = 0x4450C2;
      return NO_EXEC_DEFAULT;
   }

   if (nsDuration(Army, spell))
   {
	  const int oldDuration = nsDuration(Army, spell);
	  const int oldMastery = activeSpellMastery[Army->group][Army->index][spell];
	  if (spellDuration > nsDuration(Army, spell))
		 nsDuration(Army, spell) = spellDuration;
	  if (spellMastery > activeSpellMastery[Army->group][Army->index][spell])
		 activeSpellMastery[Army->group][Army->index][spell] = spellMastery;

      if (external)
      {
         NewSpellsStatusContextV1 context = {};
         context.size = sizeof(context);
         context.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
         context.combatManager = pCombatManager;
         context.stack = Army;
         context.casterHero = *reinterpret_cast<hero**>(c->ebp + 0x14);
         context.spellId = spell;
         context.mastery = activeSpellMastery[Army->group][Army->index][spell];
         context.duration = nsDuration(Army, spell);
         context.event = NEWSPELLS_STATUS_APPLY;
         const int32_t result = invokeStatusCallback(*external,
            external->descriptor.OnStatusApply, context);
         if (result == NEWSPELLS_PROVIDER_UNSUPPORTED)
            failExternalSpellClosed(*external);
         else if (result != NEWSPELLS_PROVIDER_COMMITTED)
         {
            nsDuration(Army, spell) = oldDuration;
            activeSpellMastery[Army->group][Army->index][spell] = oldMastery;
         }
      }
	  c->return_address = 0x4450C2;
   }
   else
   {
	  ++Army->numSpellInfluences;
	  nsDuration(Army, spell) = spellDuration;
	  activeSpellMastery[Army->group][Army->index][spell] = spellMastery;
	  c->Push(c->edi);
	  c->edi = o_Spell[spell].effect[spellMastery];
	  c->eax = spellMastery;
	  c->return_address = 0x4446E5;
   }

   return NO_EXEC_DEFAULT;
}

int __stdcall mobilitySetWin(LoHook* h, HookContext* c)
{
   hero* attHero = *(hero**)(c->ebp + 0xC);
   hero* defHero = (hero*)c->ebx;
   hero* winHero = 0;
   int winnerSide = c->eax;

   if (attHero && winnerSide == ATTACKER)
	  winHero = attHero;
   else if (defHero && winnerSide == DEFENDER)
	  winHero = defHero;

   if (hasValidHeroId(winHero))
	  heroAdvInfoEx[winHero->id].mobilityWonBattle = true;
  
   return EXEC_DEFAULT;
}

/*
int __stdcall SetSeed2(LoHook* h, HookContext* c)
{
	c->eax = 1654272046;
	return EXEC_DEFAULT;
}
*/

// Debug AI
#ifdef NEWSPELLS_DEBUG
void writeInfo(const char* headerStr, const int side, const type_AI_enemy_data* info)
{
   char buffer[2048];
   char infoStr[2048];
   sprintf(infoStr, "{%s}\n\n", headerStr);

   for (int i = 0; i < pCombatManager->stacks_count[side]; ++i)
   {
	  army* Army = (army*)&pCombatManager->stack[side][i];

	  sprintf(buffer, "{%d} %s\n{%d} %s\n{%d} | {%d} | {%d}\n\n",
		 Army->numTroops,
		 Army->numTroops > 1 ? Army->sMonInfo.m_plural_name : Army->sMonInfo.m_name,
		 info[i].enemy ? info[i].enemy->numTroops : -1,
		 info[i].enemy ? (info[i].enemy->numTroops > 1 ? o_pCreatureInfo[info[i].enemy->armyType].name_plural : o_pCreatureInfo[info[i].enemy->armyType].name_single) : "(none)",
		 info[i].count,
		 info[i].damage,
		 info[i].total_damage);
	  
	  strcat(infoStr, buffer);
   }

   debugStr("%s", infoStr);
}

void __stdcall AIShowInfo(HiHook* h, type_AI_spellcaster *spellcaster, CombatManager *combatMgr, int side, bool is_creature_spell)
{
   CALL_4(void, __thiscall, h->GetDefaultFunc(), spellcaster, combatMgr, side, is_creature_spell);

   writeInfo("Melee", side, &spellcaster->melee_enemies[0]);
   writeInfo("Shooting", side, &spellcaster->ranged_enemies[0]);
   writeInfo("Most powerful", side, &spellcaster->worst_enemies[0]);
}
#endif

#include "NsDurationHooks.h"

void writeSpellCountPatches()
{
   _PI->WriteDword      (0x432A43, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x432D1E, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x52A9B6, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x4397E6, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x43C6F2, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x43C21B, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x447551, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x447C7D, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x447CC8, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x4F50CE, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x4F5114, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x4C92C5, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x4C9347, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x4C93C0, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x527B08, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x534C4B, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5353D5, SPELLS_NUM);
   _PI->WriteDword      (0x59CCDD, SPELLS_NUM * sizeof(_BookSpell_));
   _PI->WriteDword      (0x59CD36, SPELLS_NUM * sizeof(_BookSpell_));
   _PI->WriteDword      (0x59CDBF, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5A1AD6, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5BEAFE, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5BEB2C, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5BEC05, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5BE512, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5BE56E, SPELLS_NUM * sizeof(_Spell_));
   _PI->WriteDword      (0x5D7464, SPELLS_NUM);
   _PI->WriteDword      (0x43E3DF, nsDurationLoopCount());

   // Anti-Magic AI walks the spell table up to this byte limit.
   if (*reinterpret_cast<const unsigned int*>(0x4447FE) == 0x2B08)
      _PI->WriteDword(0x4447FE, nsDurationLoopCount() * sizeof(_Spell_));
}

// ERA loads the Lang json after the plugins, so everything that reads it waits for OnAfterWoG.
// The exe code these patches change runs later, at game initialization.
void __stdcall nsAfterWogStartup(Era::TEvent* e)
{
   nsLoadDataSpells();
   nsReadOptions();
   activeSpellCount = getConfiguredSpellCount();
   writeSpellCountPatches();

   if (Era::SetAssocVarIntValue)
      Era::SetAssocVarIntValue("NewSpells.SpellCount", activeSpellCount);
   char countText[32];
   sprintf(countText, "Spell count %d", activeSpellCount);
   Era::WriteLog("NewSpells", "Startup", countText);

   nsRegisterDataProvider();
   nsInstallOptions();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
   static bool plugin_On = false;
   if ( DLL_PROCESS_ATTACH == ul_reason_for_call )
   {
      if ( !plugin_On )
      {
         plugin_On = true;
		 DisableThreadLibraryCalls(hModule);
		 Era::ConnectEra(hModule, "HD.Plugin.H3.NewSpells");
         _P = GetPatcher();
         if (!_P)
			return FALSE;
         _PI = _P->CreateInstance("HD.Plugin.H3.NewSpells");
		 if (!_PI)
			return FALSE;

#ifdef NEWSPELLS_BMG_BASELINE_PROBE
         // Test-only control: no New Spells runtime modifications. Start the
         // same parser/stack fixture at the ordinary text-init continuation.
         _PI->WriteLoHook(0x4EE1C1, RunBmgBaselineProbe);
         return TRUE;
#endif

#ifdef NEWSPELLS_CEILING_NATIVE_PROBE
         Era::RegisterHandler(ceilingProbeAfterPlugins, "OnAfterLoadEraPlugins");
         Era::RegisterHandler(ceilingProbeAfterWog, "OnAfterWoG");
         Era::RegisterHandler(ceilingProbeBeforeErm, "OnBeforeErm");
         ceilingProbeTranslations("DllMain");
#endif
         Era::RegisterHandler(nsAfterWogStartup, "OnAfterWoG");
         initializeIndirectTableTails();

         if (!validateNativeErmProfile())
         {
            Era::WriteLog("NewSpells", "Native ERM support",
               "Unsupported or conflicting WoG/ERA receiver profile; plugin was not installed.");
            return FALSE;
         }

         if (!bindCanonicalWogSpellTable())
         {
            Era::WriteLog("NewSpells", "Native ERM support",
               "Could not bind ERA's spell-table relocation to WoG's canonical table.");
            return FALSE;
         }

         if (!installCoreNativeErmHooks())
         {
            Era::WriteLog("NewSpells", "Native ERM support",
               "Could not install the native ERM receiver hook bundle.");
            if (nativeHookRollbackUnsafe)
            {
               Era::WriteLog("NewSpells", "Native ERM support",
                  "Rollback was incomplete; keeping the DLL resident to protect installed callbacks.");
               return TRUE;
            }
            return FALSE;
         }

         if (!installSpellBoundHooks())
            Era::WriteLog("NewSpells", "Spell bounds",
               "Not every spell-count compare is hooked; the unhooked sites keep the exe's bound of 70.");

         Era::RegisterHandler(saveSpellTableVersion, "OnSavegameWrite");
         Era::RegisterHandler(loadSpellTableVersion, "OnSavegameRead");
         Era::RegisterHandler(saveMapDisabledSpells, "OnSavegameWrite");
         Era::RegisterHandler(loadSavedMapDisabledSpells, "OnSavegameRead");
         // Populate WoG's canonical 200-record spell table after text/media init.
         _PI->WriteLoHook     (0x4EE1C1, afterInit);
               
         writeHeroSpellHooks();
         writeDisabledSpellHooks();
         writeDurationHooks();
         writeDurationLoopHooks();
         nsWriteVirtualLodHooks();
         nsScanLooseFiles();
         // ===============================================================
         // ------------------------- Battle AI ---------------------------
         // ---------------------------------------------------------------
         // Quick-combat's native simulator knows only the core spell
         // implementations. Keep provider slots out of its flag-only loop;
         // provider tactical AI is callback-owned.
         _PI->WriteDword      (0x425E98,
            DEFAULT_SPELLS_NUM * sizeof(_Spell_));
		 // ---------------------------------------------------------------
         
         // ===============================================================
         // ----------------------- Adventure AI --------------------------
         // ---------------------------------------------------------------
         // 0x56B344..0x56B349 is owned by the ERA cursed-ground AI fix.
         // 0x56B34A begins the adjacent `test al, al` instruction.
         const unsigned char aiTownPortalTest[] = {0x84, 0xC0};
         if (memcmp(reinterpret_cast<const void*>(0x56B34A), aiTownPortalTest,
                    sizeof(aiTownPortalTest)) == 0)
         {
            _PI->WriteLoHook(0x56B34A, aiTownPortalExpandedSpellbook);
         }
		 // AI Value of Pyramid
         const unsigned char moveHeroAiSignature[] =
            {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x28, 0x53, 0x56,
             0x8B, 0xF1, 0x57, 0xC7, 0x45, 0xFC, 0x00, 0x7D};
         if (memcmp(reinterpret_cast<const void*>(0x526C90),
                    moveHeroAiSignature, sizeof(moveHeroAiSignature)) == 0)
            adventureAiProviderHookInstalled = _PI->WriteHiHook(0x526C90,
               SPLICE_, EXTENDED_, FASTCALL_,
               evaluateExternalAdventureSpellsForAi) != 0;
         // ===============================================================
         
         // ===============================================================
         // ------------------------ Battle AI ----------------------------
         // ---------------------------------------------------------------
		 _PI->WriteLoHook	  (0x43975F, getCancelValue);
         // Master Genie AI Spell Weighting
         // AI Quick Battle
		 // Treat Fear as an incapacitating effect during AI planning. Install
		 // only on the verified SoD/ERA instruction profile.
		 const unsigned char fearAiSite1[] = {0x8A, 0x56, 0x1D};
		 const unsigned char fearAiSite2[] = {0x05, 0x48, 0x05, 0x00, 0x00};
		 const unsigned char fearAiSite3[] = {0x8B, 0x83, 0x84, 0x00, 0x00, 0x00};

		 if (memcmp(reinterpret_cast<const void*>(0x43A371), fearAiSite1, sizeof(fearAiSite1)) == 0)
			_PI->WriteLoHook(0x43A371, AI_TreatFearEffect);

		 if (memcmp(reinterpret_cast<const void*>(0x43A4FD), fearAiSite2, sizeof(fearAiSite2)) == 0)
			_PI->WriteLoHook(0x43A4FD, AI_TreatFearEffect2);

		 if (memcmp(reinterpret_cast<const void*>(0x43A603), fearAiSite3, sizeof(fearAiSite3)) == 0)
			_PI->WriteLoHook(0x43A603, AI_TreatFearEffect3);
		 // ===============================================================
                           
         // Can cast
         
         
         // Cheats in battle
         
                  
         // Scholar Secondary Skill

		 // Load Game

         _PI->WriteByte       (0x4E67AC, DEFAULT_SPELLS_NUM);
         
         // Cheat Menu?
                           
         // Init spells
		 _PI->WriteLoHook     (0x4C2625, initSpells);
         
		 // Shrine spells
		 _PI->WriteHiHook     (0x4C9260, SPLICE_, DIRECT_, THISCALL_, FillShrine);

		 // Pyramids
                 
         _PI->WriteByte       (0x4CEC4F, DEFAULT_SPELLS_NUM);
         
         // Tome of Air Magic
         _PI->WriteDword      (0x4D962D, DEFAULT_SPELLS_NUM * sizeof(_Spell_));
         // Tome of Fire Magic
         _PI->WriteDword      (0x4D9681, DEFAULT_SPELLS_NUM * sizeof(_Spell_));
         // Tome of Water Magic
         _PI->WriteDword      (0x4D96D6, DEFAULT_SPELLS_NUM * sizeof(_Spell_));
         // Tome of Earth Magic
         _PI->WriteDword      (0x4D972E, DEFAULT_SPELLS_NUM * sizeof(_Spell_));
         // Spellbinder's Hat
         _PI->WriteDword      (0x4D9771, DEFAULT_SPELLS_NUM * sizeof(_Spell_));
         
         // Scholars
         
		 
		 // RMG
         _PI->WriteLoHook     (0x54AE0F, RMGDisableSpells);
         
         // RMG Spell Scrolls
         _PI->WriteLoHook     (0x5353E8, RMGDisableSpellsInScrollsA);
         _PI->WriteLoHook     (0x535417, RMGDisableSpellsInScrollsB);
         
         // Spell Book

		 // Add Spell
		 _PI->WriteHiHook     (0x4D95A0, SPLICE_, DIRECT_, THISCALL_, AddSpell);
		 
         // Cast Spell


		 // ===============================================================
         // -------------------------- Mage Guild -------------------------
         // ---------------------------------------------------------------
		 
		 // Town setup
		 _PI->WriteByte       (0x5BEA05, 0x30); // Now we have the equivalent of std::bitset<SPELLS_MAX> on the stack
		 _PI->WriteDword      (0x5BEA12, 4); // Let's initialize it with zeroes
		 
		 int newBitsetOffset[] = {0x5BEA67, 0x5BEAB2, 0x5BEACF, 0x5BEB50, 0x5BEB68, 0x5BEBB7, 0x5BEBD6, 0x5BEC23, 0x5BEC3B};
		 for (std::size_t i = 0; i < sizeof(newBitsetOffset) / sizeof(int); ++i)
            _PI->WriteByte(newBitsetOffset[i], -0x30);

		 _PI->WriteLoHook	  (0x5BEA24, expandMayAppearSpells);
		 _PI->WriteLoHook	  (0x5BE518, expandMayAppearSpells); // Aurora Borealis
		 _PI->WriteLoHook	  (0x5BE52D, expandMayAppearSpells); // Aurora Borealis
		 _PI->WriteLoHook	  (0x5BEB15, expandMustAppearSpells);
         // ===============================================================
                 
         // Conflux Grail
         
         // ===============================================================
         // ------------- New Game.disabled_spells[140] field -------------
         // ---------------------------------------------------------------
         _PI->WriteByte       (0x4C254C, 4); // Titan's Lightning Bolt
         _PI->WriteByte       (0x4C25F1, 4); // Titan's Lightning Bolt
         // ===============================================================
                         

         // ===============================================================
         // -------------------------- Combat -----------------------------
         // ---------------------------------------------------------------
         _PI->WriteDword      (0x59EFE4, (int)&spellIndirectTableA);
         _PI->WriteDword      (0x5A0659, (int)&spellIndirectTableB);

		 combatProviderTargetRoutingInstalled =
			_PI->WriteLoHook(0x59EFE8, spellTableAHelper) != 0;
		 const bool combatAiDispatcherInstalled =
			_PI->WriteLoHook(0x43B797, considerSpellDispatcher) != 0;
		 _PI->WriteLoHook	  (0x5A4CB3, showAreaAnim);
		 _PI->WriteLoHook	  (0x5A4D15, goldenTouchAreaAnim);
		 _PI->WriteLoHook	  (0x5A0E70, goldenTouchAddGoldSingleTarget);
		 _PI->WriteLoHook	  (0x5A4DF2, goldenTouchAddGold);
		 _PI->WriteLoHook	  (0x469941, appendCombatLogInfo);
		 // BattleReplay owns a HiHook at the dialog entry (0x46FE20). Hook a
		 // verified instruction boundary inside the original implementation so
		 // both plugins remain in the call chain instead of mixing hook types at
		 // the same address.
		 const unsigned char battleResultDialogSignature[] =
			{0x56, 0x57, 0x6A, 0x10, 0x68, 0x31, 0x02, 0x00, 0x00};
		 if (memcmp(reinterpret_cast<const void*>(0x46FE3E),
			battleResultDialogSignature, sizeof(battleResultDialogSignature)) == 0)
			_PI->WriteLoHook(0x46FE3E, notifyPlayerAfterBattle);
		          
		 // army::SetSpellInfluence() switch
         _PI->WriteDword      (0x4446FD, (int)&SetSpellInfluenceTable);
         
         _PI->WriteDword      (0x44427A, (int)&spellIndirectTableD);
         _PI->WriteDword      (0x44A264, (int)&spellIndirectTableE);
         _PI->WriteDword      (0x43B793, (int)&spellIndirectTableF);
         const bool combatAiFunctionHookInstalled = _PI->WriteHiHook(
            0x43B2E0, SPLICE_, EXTENDED_, THISCALL_,
            get_enchantment_function) != 0;
         combatAiProviderHooksInstalled = combatAiDispatcherInstalled &&
            combatAiFunctionHookInstalled;
		 const bool statusInfluenceProviderHookInstalled =
			_PI->WriteLoHook(0x44467E, setSpellInfluence) != 0;
         // ===============================================================
                 
         // ===============================================================
         // -------------- New army.spellInfluence[162] field -------------
         // ---------------------------------------------------------------
         _PI->WriteDword      (0x43787A, 162);
         _PI->WriteDword      (0x43D314, 162);
         // ===============================================================
        
         // Fear, Poison, Disease, Age, ...
         const bool statusApplyEntryHookInstalled =
            _PI->WriteLoHook(0x444701, applySpell) != 0;
         statusApplyProviderHooksInstalled =
            statusInfluenceProviderHookInstalled &&
            statusApplyEntryHookInstalled;
         statusRemoveProviderHookInstalled =
            _PI->WriteLoHook(0x44427E, resetSpell) != 0;
         // Poison every round, no retaliations for Fear at Expert
         statusRoundProviderHookInstalled =
            _PI->WriteLoHook(0x446F11, newRoundSpellSettings) != 0;
         // Creature's cast
         // Zombie's Disease on Magic Plains
         combatProviderPrecheckInstalled =
            _PI->WriteLoHook(0x5A01E5, zombieDiseaseOnMagicPlains) != 0;
         // Intercept only Dispel's two direct CancelIndividualSpell calls.
         // Unlike a hook on army::CancelIndividualSpell itself, these seams
         // never see Cure, duration expiry, ERM removal, or unrelated native
         // cleanup.  Both call instructions are verified independently
         // against the current ERA executable before the capability is
         // advertised to providers.
         const unsigned char singleTargetDispelSeam[] =
            {0x74, 0x08, 0x56, 0x8B, 0xCF,
             0xE8, 0x2C, 0x29, 0xEA, 0xFF};
         const unsigned char massDispelSeam[] =
            {0x74, 0x08, 0x57, 0x8B, 0xCE,
             0xE8, 0x9E, 0x28, 0xEA, 0xFF};
         if (memcmp(reinterpret_cast<const void*>(0x5A18FA),
                    singleTargetDispelSeam,
                    sizeof(singleTargetDispelSeam)) == 0 &&
             memcmp(reinterpret_cast<const void*>(0x5A1988),
                    massDispelSeam, sizeof(massDispelSeam)) == 0)
         {
            // The two short jumps skip spell IDs >= 71. Remove them only as
            // part of the fully verified pair so built-in Poison and external
            // status slots enter these same two cancellation call sites.
            Patch* dispelPatches[] =
            {
               _PI->CreateHiHook(0x5A18FF, CALL_, EXTENDED_, THISCALL_,
                                 dispelExternalSpell),
               _PI->CreateHiHook(0x5A198D, CALL_, EXTENDED_, THISCALL_,
                                 dispelExternalSpell),
               _PI->CreateHexPatch(0x5A18FA, "90 90"),
               _PI->CreateHexPatch(0x5A1988, "90 90")
            };
            const std::size_t dispelPatchCount = sizeof(dispelPatches) /
               sizeof(dispelPatches[0]);
            bool allDispelPatchesCreated = true;
            for (std::size_t i = 0; i < dispelPatchCount; ++i)
               allDispelPatchesCreated = allDispelPatchesCreated &&
                  dispelPatches[i] != 0;

            std::size_t appliedDispelPatchCount = 0;
            if (allDispelPatchesCreated)
            {
               // Apply both verified CALL hooks before removing either skip.
               // A failed hook can therefore never expose IDs >= 71 to an
               // unowned Dispel cancellation seam.
               for (; appliedDispelPatchCount < dispelPatchCount;
                    ++appliedDispelPatchCount)
               {
                  dispelPatches[appliedDispelPatchCount]->Apply();
                  if (!dispelPatches[appliedDispelPatchCount]->IsApplied())
                     break;
               }
               dispelProviderHooksInstalled =
                  appliedDispelPatchCount == dispelPatchCount;
            }

            if (!dispelProviderHooksInstalled)
            {
               for (std::size_t i = appliedDispelPatchCount; i > 0; --i)
                  if (dispelPatches[i - 1] && dispelPatches[i - 1]->IsApplied())
                     dispelPatches[i - 1]->Undo();
               for (std::size_t i = 0; i < dispelPatchCount; ++i)
                  if (dispelPatches[i] && !dispelPatches[i]->IsApplied())
                     dispelPatches[i]->Destroy();
            }
         }
         _PI->WriteCodePatch  (0x43972C, "%n", 12); // type_AI_spellcaster::get_cancel_value()

         // Allow to cure new spells
         cureProviderHookInstalled =
            _PI->WriteLoHook(0x4462FD, cureNewSpells) != 0;

         // Death Cloud, Incineration
		 _PI->WriteLoHook     (0x41FBEB, get_area_effect_new);
         // The exe's switch spans spells 10..80: "lea eax, [edx-10]" then "cmp eax, 70".
         const unsigned char battleDispatcherSignature[] = {0x8D, 0x42, 0xF6, 0x83, 0xF8, 0x46};
		 if (memcmp(reinterpret_cast<const void*>(0x5A0649), battleDispatcherSignature,
				sizeof(battleDispatcherSignature)) == 0)
			combatProviderDispatcherInstalled =
				_PI->WriteLoHook(0x5A0649, battleSpellDispatcher) != 0;
		 const unsigned char battleEpilogueSignature[] =
			{0x8D, 0xBB, 0xD4, 0x54, 0x00, 0x00};
		 if (memcmp(reinterpret_cast<const void*>(0x5A2368),
			battleEpilogueSignature, sizeof(battleEpilogueSignature)) == 0)
			combatProviderEpilogueInstalled = _PI->WriteLoHook(0x5A2368,
				clearCompletedExternalCombatTransaction) != 0;
		 _PI->WriteLoHook     (0x5A4DE7, applyIncinerationSpecialFeature);
		 //_PI->WriteHiHook     (0x447050, SPLICE_, DIRECT_, THISCALL_, get_resurection_size);
		 		 
         // Death Blow
         _PI->WriteLoHook     (0x4435A9, DeathBlow);

         // Drain Life
         _PI->WriteLoHook     (0x440903, DrainLife);
         _PI->WriteLoHook     (0x44096F, calcDrainLifeHeal);

         // Summon Sprite, Summon Magic Elemental, Summon Firebird
         _PI->WriteLoHook     (0x59F889, checkSummonedCreaturesType);
         _PI->WriteLoHook     (0x5A74D6, skipNonElementals);
         _PI->WriteLoHook     (0x5A7516, setSummonedCreaturesNumber);
         _PI->WriteHiHook     (0x5A96B0, SPLICE_, DIRECT_, THISCALL_, AbleToSummonElemental);
         _PI->WriteHiHook     (0x5A9670, SPLICE_, DIRECT_, FASTCALL_, get_elemental_type);

		 // Adventure Spells
		 adventureCastProviderHookInstalled =
			_PI->WriteLoHook(0x41C663, castAdventureSpell) != 0;
		 const unsigned char eyeHookSignature[] = {0x50, 0xE8, 0x79, 0x15, 0xF8, 0xFF};
		 if (memcmp(reinterpret_cast<const void*>(0x4915B1), eyeHookSignature,
				sizeof(eyeHookSignature)) == 0)
			_PI->WriteLoHook(0x4915B1, eyeOfTheMagi);
		 _PI->WriteHiHook     (0x50CEA0, SPLICE_, EXTENDED_, THISCALL_, setMouseCursor);
		 _PI->WriteLoHook     (0x4AE130, mobilitySetWin);
		          
         // Stack's Magic Vulnerability
         _PI->WriteLoHook     (0x44A268, getStackMagicVulnerability);
         
         // Artifacts (incl. Spell Scrolls)
         _PI->WriteHiHook     (0x4D9840, SPLICE_, EXTENDED_, THISCALL_, UpdateSpellsFromArtifacts);
         _PI->WriteLoHook     (0x4D988D, artifactScrollSpell);

         _PI->WriteHiHook     (0x4425A0, SPLICE_, DIRECT_, THISCALL_, can_attack);
		 _PI->WriteHiHook     (0x4C7CA0, SPLICE_, EXTENDED_, THISCALL_, beforeNewDayStart);
		 _PI->WriteHiHook     (0x4C2110, SPLICE_, EXTENDED_, THISCALL_, beforeNewGameStart);
		          
         // ===============================================================
         // ---------------------------- Fear -----------------------------
         // ---------------------------------------------------------------
         // Set cursor for human players
         _PI->WriteHiHook     (0x475DC0, SPLICE_, EXTENDED_, THISCALL_, setCursorForFearSpell);
         // Not allowing to attack under Fear
         installFearMeleeGuardHook();
         _PI->WriteLoHook     (0x41F25C, skipShootingUnderFear);
         // Getting defense modifier (common for Blind, Paralyze, Fear)
         _PI->WriteHiHook     (0x4422B0, SPLICE_, EXTENDED_, THISCALL_, GetEffectiveDefenseAgainst);
		 _PI->WriteHiHook     (0x4438B0, SPLICE_, EXTENDED_, THISCALL_, ComputeAttackerDamageReduction);
         // ===============================================================

         const bool battleStartProviderHookInstalled = _PI->WriteHiHook(
            0x4631E0, SPLICE_, EXTENDED_, THISCALL_,
            combatManagerLoadArmies) != 0;
		 const bool battleEndProviderHookInstalled = _PI->WriteHiHook(
			0x476DA0, SPLICE_, EXTENDED_, THISCALL_,
			combatManagerDoVictory) != 0;
         battleLifecycleProviderHooksInstalled =
            battleStartProviderHookInstalled && battleEndProviderHookInstalled;

		 // Hour of Power. Bless
		 _PI->WriteHiHook     (0x442410, SPLICE_, DIRECT_, THISCALL_, get_average_damage);
		 _PI->WriteLoHook	  (0x4424A9, getAverageDamageInt);
		 _PI->WriteLoHook	  (0x4429B4, getUnitCombatValue);
		 _PI->WriteLoHook	  (0x442F64, randomizeBasicDamage);
		 _PI->WriteLoHook	  (0x443480, computeAttackerDamageBonuses);
		 _PI->WriteLoHook	  (0x492F9F, getAttackDamageHintString);
		 // Hour of Power. Bloodlust
		 _PI->WriteLoHook	  (0x44215A, getAdjustedAttack_Bloodlust);
		 _PI->WriteLoHook	  (0x43BF8D, getOgreMageValue);
		 // Hour of Power. Fortune. erm_hooker owns the preceding six-byte
		 // load at 0x43DC9F; override EAX at the following TEST instead.
		 const unsigned char fortuneTest[] = {0x85, 0xC0};
		 if (memcmp(reinterpret_cast<const void*>(0x43DCA5), fortuneTest,
					sizeof(fortuneTest)) == 0)
		 {
			_PI->WriteLoHook(0x43DCA5, hourOfPower_Fortune);
		 }
		 // Hour of Power. Slayer
		 _PI->WriteLoHook	  (0x44216F, getAdjustedAttack_Slayer);
		 // Hour of Power. Spell immunities
		 combatProviderWorkChanceInstalled = _PI->WriteHiHook(
			0x5A83A0, SPLICE_, EXTENDED_, THISCALL_,
			SpellCastWorkChance) != 0;
		 
		 // Speed
		 _PI->WriteHiHook     (0x4489F0, SPLICE_, EXTENDED_, THISCALL_, GetSpeed);
		 		 
		 int speedModJmpAddr[] = {0x441CE3, 0x441E06, 0x44857E, 0x4485BC, 0x4485FC, 0x44867A};
		 for (std::size_t i = 0; i < sizeof(speedModJmpAddr) / sizeof(int); ++i)
            _PI->WriteHexPatch(speedModJmpAddr[i], "90 90");

		 // ===============================================================
         // ---------------------- Save & Load Game -----------------------
         // ---------------------------------------------------------------
		 _PI->WriteLoHook     (0x4D7D44, loadHeroAdvInfoEx);
		 _PI->WriteLoHook     (0x4D83B3, saveHeroAdvInfoEx);
		 
		 _PI->WriteDword	  (0x41814E, (int&)gmnExt); // .GM%d
		 _PI->WriteDword	  (0x418139, (int&)cgmExt); // .CGM
		 _PI->WriteDword	  (0x4BEC71, (int&)gmnStr); // %s.GM%d
		 _PI->WriteDword	  (0x4BEC27, (int&)cgmStr); // CGM
		 _PI->WriteDword	  (0x4BEC3B, (int&)tgmStr); // TGM
		 _PI->WriteDword	  (0x6834AC, (int&)gmnMsk); // *.gm?
		 _PI->WriteDword	  (0x6834B0, (int&)cgmMsk); // *.cgm
		 _PI->WriteDword	  (0x6834B8, (int&)tgmMsk); // *.tgm
		 // ===============================================================

		 // Debug AI
#ifdef NEWSPELLS_DEBUG
		 _PI->WriteHiHook     (0x436610, SPLICE_, EXTENDED_, THISCALL_, AIShowInfo);
#endif

         // Publish only after initialization has finished, so no consumer can
         // retain a callback pointer from a DLL that subsequently fails load.
         providerRuntimeCapabilities = NEWSPELLS_CAP_ALL_V1;
         if (!combatProviderPrecheckInstalled ||
             !combatProviderTargetRoutingInstalled ||
             !combatProviderWorkChanceInstalled ||
             !combatProviderDispatcherInstalled ||
             !combatProviderEpilogueInstalled)
            providerRuntimeCapabilities &= ~(NEWSPELLS_CAP_COMBAT_TARGET |
               NEWSPELLS_CAP_COMBAT_CAST | NEWSPELLS_CAP_COMBAT_AI);
         if (!combatAiProviderHooksInstalled)
            providerRuntimeCapabilities &= ~NEWSPELLS_CAP_COMBAT_AI;
         if (!adventureCastProviderHookInstalled)
            providerRuntimeCapabilities &= ~(NEWSPELLS_CAP_ADVENTURE_CAST |
               NEWSPELLS_CAP_ADVENTURE_AI);
         if (!adventureAiProviderHookInstalled)
            providerRuntimeCapabilities &= ~NEWSPELLS_CAP_ADVENTURE_AI;
         if (!statusApplyProviderHooksInstalled)
            providerRuntimeCapabilities &= ~NEWSPELLS_CAP_STATUS_APPLY;
         if (!statusRoundProviderHookInstalled)
            providerRuntimeCapabilities &= ~NEWSPELLS_CAP_STATUS_ROUND;
         if (!statusRemoveProviderHookInstalled)
            providerRuntimeCapabilities &= ~NEWSPELLS_CAP_STATUS_REMOVE;
         if (!cureProviderHookInstalled || !dispelProviderHooksInstalled)
            providerRuntimeCapabilities &= ~NEWSPELLS_CAP_CURE_DISPEL;
         if (!battleLifecycleProviderHooksInstalled)
            providerRuntimeCapabilities &= ~NEWSPELLS_CAP_BATTLE_LIFECYCLE;
         newSpellsProviderRegistryV1.capabilities = providerRuntimeCapabilities;
         publishAiSpellQueryInterop();
         publishSpellProviderRegistry();
      }
   }

   return TRUE;
}

#ifdef NEWSPELLS_BMG_NATIVE_PROBE
#include "../tests/ErmBattleFieldsNativeProbe.inl"
#endif
#ifdef NEWSPELLS_CEILING_NATIVE_PROBE
#include "../tests/SpellCeilingNativeProbe.inl"
#endif
