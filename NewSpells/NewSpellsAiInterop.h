#ifndef NEWSPELLS_AI_INTEROP_H
#define NEWSPELLS_AI_INTEROP_H

#include <stddef.h>
#include <stdint.h>

#define NEWSPELLS_AI_SPELL_QUERY_VARIABLE_V1 \
   "HD.Plugin.H3.NewSpells.AI.SpellQuery.v1"
#define NEWSPELLS_AI_INTEROP_ABI_VERSION_V1 1u
#define NEWSPELLS_AI_CAPABILITY_QUERY_HERO_SPELL 0x00000001u

#pragma pack(push, 4)

typedef struct AiSpellStateV1
{
   uint32_t size;
   int32_t known;
   int32_t enabled;
   int32_t mastery;
   int32_t effectiveManaCost;
   int32_t currentMana;
} AiSpellStateV1;

typedef struct NewSpellsAiInteropV1
{
   uint32_t size;
   uint32_t abiVersion;
   uint32_t capabilities;
   int32_t (__stdcall *QueryHeroSpell)(const void* combatManager,
      int32_t casterSide, int32_t spellId, AiSpellStateV1* outState);
} NewSpellsAiInteropV1;

#pragma pack(pop)

#ifdef __cplusplus
static_assert(sizeof(AiSpellStateV1) == 24,
   "New Spells AI spell-state ABI mismatch");
static_assert(offsetof(AiSpellStateV1, size) == 0,
   "New Spells AI spell-state size offset mismatch");
static_assert(offsetof(AiSpellStateV1, known) == 4,
   "New Spells AI spell-state known offset mismatch");
static_assert(offsetof(AiSpellStateV1, enabled) == 8,
   "New Spells AI spell-state enabled offset mismatch");
static_assert(offsetof(AiSpellStateV1, mastery) == 12,
   "New Spells AI spell-state mastery offset mismatch");
static_assert(offsetof(AiSpellStateV1, effectiveManaCost) == 16,
   "New Spells AI spell-state mana-cost offset mismatch");
static_assert(offsetof(AiSpellStateV1, currentMana) == 20,
   "New Spells AI spell-state current-mana offset mismatch");
static_assert(sizeof(void*) == 4,
   "New Spells AI interop is a Win32 ABI");
static_assert(sizeof(NewSpellsAiInteropV1) == 16,
   "New Spells AI provider ABI mismatch");
static_assert(offsetof(NewSpellsAiInteropV1, QueryHeroSpell) == 12,
   "New Spells AI provider callback offset mismatch");

namespace NewSpellsAiInteropDetail
{
static const size_t kGameDisabledSpellsOffset = 4u;

inline int32_t SpellEnabledFromUnifiedGameState(
   const void* const game, const int32_t spellId)
{
   const uint8_t* const bytes = static_cast<const uint8_t*>(game);
   return bytes && spellId >= 0
      ? (bytes[kGameDisabledSpellsOffset +
            static_cast<size_t>(spellId)] == 0 ? 1 : 0)
      : 0;
}
}
#endif

#endif
