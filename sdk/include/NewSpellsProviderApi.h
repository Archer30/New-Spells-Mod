#ifndef NEWSPELLS_PROVIDER_API_H
#define NEWSPELLS_PROVIDER_API_H

#include <stddef.h>
#include <stdint.h>

#define NEWSPELLS_PROVIDER_REGISTRY_VARIABLE_V1 \
   "HD.Plugin.H3.NewSpells.SpellProviderRegistry.v1"
#define NEWSPELLS_PROVIDER_ABI_VERSION_V1 1u
#define NEWSPELLS_EXTERNAL_SPELL_FIRST_ID 96
#define NEWSPELLS_EXTERNAL_SPELL_LAST_ID 126

/* Every callback returns one of these values. Unknown values fail closed. */
enum NewSpellsProviderResultV1
{
   NEWSPELLS_PROVIDER_UNSUPPORTED = 0,
   NEWSPELLS_PROVIDER_DENIED = 1,
   NEWSPELLS_PROVIDER_CANCELLED = 2,
   NEWSPELLS_PROVIDER_COMMITTED = 3
};

enum NewSpellsProviderCapabilityV1
{
   NEWSPELLS_CAP_ADVENTURE_CAST       = 0x00000001u,
   NEWSPELLS_CAP_COMBAT_TARGET        = 0x00000002u,
   NEWSPELLS_CAP_COMBAT_CAST          = 0x00000004u,
   NEWSPELLS_CAP_STATUS_APPLY         = 0x00000008u,
   NEWSPELLS_CAP_STATUS_ROUND         = 0x00000010u,
   NEWSPELLS_CAP_STATUS_REMOVE        = 0x00000020u,
   NEWSPELLS_CAP_CURE_DISPEL          = 0x00000040u,
   NEWSPELLS_CAP_BATTLE_LIFECYCLE     = 0x00000080u,
   NEWSPELLS_CAP_CREATURE_CAST        = 0x00000100u,
   NEWSPELLS_CAP_ERM_CAST             = 0x00000200u,
   NEWSPELLS_CAP_COMBAT_AI            = 0x00000400u,
   NEWSPELLS_CAP_ADVENTURE_AI         = 0x00000800u,
   NEWSPELLS_CAP_ALL_V1               = 0x00000FFFu
};

enum NewSpellsProviderFlagsV1
{
   NEWSPELLS_PROVIDER_HUMAN_ONLY = 0x00000001u
};

enum NewSpellsCastSourceV1
{
   NEWSPELLS_SOURCE_HERO = 0,
   NEWSPELLS_SOURCE_CREATURE = 1,
   NEWSPELLS_SOURCE_ERM = 2
};

enum NewSpellsStatusEventV1
{
   NEWSPELLS_STATUS_APPLY = 0,
   NEWSPELLS_STATUS_ROUND = 1,
   NEWSPELLS_STATUS_REMOVE = 2,
   NEWSPELLS_STATUS_CURE = 3,
   NEWSPELLS_STATUS_DISPEL = 4,
   NEWSPELLS_STATUS_CURE_DISPEL = NEWSPELLS_STATUS_CURE
};

enum NewSpellsBattleEventV1
{
   NEWSPELLS_BATTLE_START = 0,
   NEWSPELLS_BATTLE_END = 1
};

#pragma pack(push, 4)

typedef struct NewSpellsAdventureContextV1
{
   uint32_t size;
   uint32_t abiVersion;
   void* hero;
   void* adventureManager;
   void* game;
   int32_t spellId;
   int32_t mastery;
   int32_t specialTerrain;
   int32_t isHuman;
   /* Provider-defined adventure target selected by EvaluateAdventureAi. */
   int32_t target;
} NewSpellsAdventureContextV1;

typedef struct NewSpellsCombatContextV1
{
   uint32_t size;
   uint32_t abiVersion;
   void* combatManager;
   void* casterHero;
   void* casterStack;
   void* targetStack;
   int32_t spellId;
   int32_t casterSide;
   int32_t targetHex;
   int32_t mastery;
   int32_t spellPower;
   int32_t source;
} NewSpellsCombatContextV1;

typedef struct NewSpellsStatusContextV1
{
   uint32_t size;
   uint32_t abiVersion;
   void* combatManager;
   void* stack;
   void* casterHero;
   int32_t spellId;
   int32_t mastery;
   int32_t duration;
   int32_t event;
} NewSpellsStatusContextV1;

typedef struct NewSpellsBattleContextV1
{
   uint32_t size;
   uint32_t abiVersion;
   void* combatManager;
   int32_t spellId;
   int32_t event;
   int32_t winningSide;
} NewSpellsBattleContextV1;

typedef struct NewSpellsAiContextV1
{
   uint32_t size;
   uint32_t abiVersion;
   void* planner;
   void* combatManager;
   void* casterHero;
   void* targetStack;
   int32_t spellId;
   int32_t casterSide;
   int32_t targetHex;
   int32_t mastery;
   int32_t spellPower;
   int32_t duration;
   int32_t score;
   int32_t castNow;
} NewSpellsAiContextV1;

typedef int32_t (__stdcall *NewSpellsAdventureCallbackV1)(
   NewSpellsAdventureContextV1* context);
typedef int32_t (__stdcall *NewSpellsCombatCallbackV1)(
   NewSpellsCombatContextV1* context);
typedef int32_t (__stdcall *NewSpellsStatusCallbackV1)(
   NewSpellsStatusContextV1* context);
typedef int32_t (__stdcall *NewSpellsBattleCallbackV1)(
   NewSpellsBattleContextV1* context);
typedef int32_t (__stdcall *NewSpellsAiCallbackV1)(
   NewSpellsAiContextV1* context);

typedef struct NewSpellsProviderSpellV1
{
   uint32_t size;
   uint32_t abiVersion;
   int32_t spellId;
   uint32_t capabilities;
   uint32_t flags;
   const char* providerKey;
   const char* spellKey;
   NewSpellsAdventureCallbackV1 ValidateAdventure;
   NewSpellsAdventureCallbackV1 CastAdventure;
   NewSpellsCombatCallbackV1 ValidateCombatTarget;
   NewSpellsCombatCallbackV1 CastCombat;
   NewSpellsStatusCallbackV1 OnStatusApply;
   NewSpellsStatusCallbackV1 OnStatusRound;
   NewSpellsStatusCallbackV1 OnStatusRemove;
   NewSpellsStatusCallbackV1 OnCureOrDispel;
   NewSpellsBattleCallbackV1 OnBattleLifecycle;
   NewSpellsCombatCallbackV1 OnCreatureCast;
   NewSpellsCombatCallbackV1 OnErmCast;
   NewSpellsAiCallbackV1 EvaluateCombatAi;
   NewSpellsAiCallbackV1 EvaluateAdventureAi;
} NewSpellsProviderSpellV1;

typedef struct NewSpellsProviderBatchV1
{
   uint32_t size;
   uint32_t abiVersion;
   const char* providerKey;
   uint32_t spellCount;
   const NewSpellsProviderSpellV1* spells;
} NewSpellsProviderBatchV1;

typedef struct NewSpellsProviderRegistryV1
{
   uint32_t size;
   uint32_t abiVersion;
   uint32_t capabilities;
   int32_t (__stdcall *RegisterProviderBatch)(
      const NewSpellsProviderBatchV1* batch);
   int32_t (__stdcall *IsRegistrationOpen)(void);
   int32_t (__stdcall *IsSpellRegistered)(int32_t spellId);
} NewSpellsProviderRegistryV1;

#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(void*) == 4, "New Spells provider API is a Win32 ABI");
static_assert(sizeof(NewSpellsAdventureContextV1) == 40,
   "New Spells adventure context ABI mismatch");
static_assert(sizeof(NewSpellsCombatContextV1) == 48,
   "New Spells combat context ABI mismatch");
static_assert(sizeof(NewSpellsStatusContextV1) == 36,
   "New Spells status context ABI mismatch");
static_assert(sizeof(NewSpellsBattleContextV1) == 24,
   "New Spells battle context ABI mismatch");
static_assert(sizeof(NewSpellsAiContextV1) == 56,
   "New Spells AI context ABI mismatch");
static_assert(sizeof(NewSpellsProviderSpellV1) == 80,
   "New Spells spell-provider descriptor ABI mismatch");
static_assert(offsetof(NewSpellsProviderSpellV1, providerKey) == 20,
   "New Spells spell-provider identity ABI mismatch");
static_assert(sizeof(NewSpellsProviderBatchV1) == 20,
   "New Spells provider-batch ABI mismatch");
static_assert(sizeof(NewSpellsProviderRegistryV1) == 24,
   "New Spells provider-registry ABI mismatch");
#endif

#endif
