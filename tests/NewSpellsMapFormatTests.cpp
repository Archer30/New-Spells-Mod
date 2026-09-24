#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "..\NewSpells\NewSpellsAiInterop.h"
#include "..\NewSpells\NewSpellsMapFormat.h"
#include "..\NewSpells\NewSpellsProviderApi.h"

namespace
{

int32_t __stdcall QueryFixture(const void*, const int32_t casterSide,
   const int32_t spellId, AiSpellStateV1* const state)
{
   if (!state || state->size < sizeof(AiSpellStateV1) || casterSide != 1 ||
       spellId != 82)
      return 0;

   AiSpellStateV1 result = {};
   result.size = sizeof(result);
   result.known = 1;
   result.enabled = 1;
   result.mastery = 2;
   result.effectiveManaCost = 19;
   result.currentMana = 37;
   *state = result;
   return 1;
}

bool AiInteropContractIsUsable()
{
   NewSpellsAiInteropV1 provider = {};
   provider.size = sizeof(provider);
   provider.abiVersion = NEWSPELLS_AI_INTEROP_ABI_VERSION_V1;
   provider.capabilities = NEWSPELLS_AI_CAPABILITY_QUERY_HERO_SPELL;
   provider.QueryHeroSpell = QueryFixture;

   AiSpellStateV1 undersizedState = {};
   undersizedState.size = sizeof(undersizedState) - 1;
   AiSpellStateV1 state = {};
   state.size = sizeof(state);
   return std::strcmp(NEWSPELLS_AI_SPELL_QUERY_VARIABLE_V1,
             "HD.Plugin.H3.NewSpells.AI.SpellQuery.v1") == 0 &&
      provider.size == 16 && provider.abiVersion == 1 &&
      provider.capabilities == 1 &&
      provider.QueryHeroSpell(0, 1, 82, &undersizedState) == 0 &&
      provider.QueryHeroSpell(0, 1, 82, &state) == 1 &&
      state.size == 24 && state.known == 1 && state.enabled == 1 &&
      state.mastery == 2 && state.effectiveManaCost == 19 &&
      state.currentMana == 37;
}

int32_t __stdcall AdventureFixture(NewSpellsAdventureContextV1* const context)
{
   return context && context->size == sizeof(*context) &&
      context->spellId == 96 && context->isHuman
      ? NEWSPELLS_PROVIDER_COMMITTED
      : NEWSPELLS_PROVIDER_DENIED;
}

int32_t __stdcall RegisterProviderFixture(
   const NewSpellsProviderBatchV1* const batch)
{
   if (!batch || batch->size != sizeof(*batch) ||
       batch->abiVersion != NEWSPELLS_PROVIDER_ABI_VERSION_V1 ||
       batch->spellCount != 1 || !batch->spells ||
       std::strcmp(batch->providerKey,
          "HD.Plugin.H3.NewSpellsExpansion") != 0)
      return 0;

   const NewSpellsProviderSpellV1& spell = batch->spells[0];
   return spell.size == sizeof(spell) &&
      spell.abiVersion == NEWSPELLS_PROVIDER_ABI_VERSION_V1 &&
      spell.spellId == 96 &&
      spell.capabilities == NEWSPELLS_CAP_ADVENTURE_CAST &&
      spell.flags == NEWSPELLS_PROVIDER_HUMAN_ONLY &&
      std::strcmp(spell.providerKey, batch->providerKey) == 0 &&
      std::strcmp(spell.spellKey, "Reinforcements") == 0 &&
      spell.ValidateAdventure && spell.CastAdventure;
}

int32_t __stdcall RegistrationOpenFixture()
{
   return 1;
}

int32_t __stdcall SpellRegisteredFixture(const int32_t spellId)
{
   return spellId == 96;
}

bool ProviderInteropContractIsUsable()
{
   NewSpellsProviderSpellV1 spell = {};
   spell.size = sizeof(spell);
   spell.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
   spell.spellId = 96;
   spell.capabilities = NEWSPELLS_CAP_ADVENTURE_CAST;
   spell.flags = NEWSPELLS_PROVIDER_HUMAN_ONLY;
   spell.providerKey = "HD.Plugin.H3.NewSpellsExpansion";
   spell.spellKey = "Reinforcements";
   spell.ValidateAdventure = AdventureFixture;
   spell.CastAdventure = AdventureFixture;

   NewSpellsProviderBatchV1 batch = {};
   batch.size = sizeof(batch);
   batch.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
   batch.providerKey = spell.providerKey;
   batch.spellCount = 1;
   batch.spells = &spell;

   NewSpellsProviderRegistryV1 registry = {};
   registry.size = sizeof(registry);
   registry.abiVersion = NEWSPELLS_PROVIDER_ABI_VERSION_V1;
   registry.capabilities = NEWSPELLS_CAP_ALL_V1;
   registry.RegisterProviderBatch = RegisterProviderFixture;
   registry.IsRegistrationOpen = RegistrationOpenFixture;
   registry.IsSpellRegistered = SpellRegisteredFixture;

   NewSpellsAdventureContextV1 context = {};
   context.size = sizeof(context);
   context.spellId = 96;
   context.isHuman = 1;

   return std::strcmp(NEWSPELLS_PROVIDER_REGISTRY_VARIABLE_V1,
             "HD.Plugin.H3.NewSpells.SpellProviderRegistry.v1") == 0 &&
      registry.size == 24 && registry.abiVersion == 1 &&
      registry.RegisterProviderBatch(&batch) == 1 &&
      registry.IsRegistrationOpen() == 1 &&
      registry.IsSpellRegistered(96) == 1 &&
      spell.CastAdventure(&context) == NEWSPELLS_PROVIDER_COMMITTED;
}

bool WriteBaseFile(const char* path, const unsigned char* bytes,
                   const DWORD size)
{
   HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL, 0);
   if (file == INVALID_HANDLE_VALUE)
      return false;

   DWORD written = 0;
   const bool result = WriteFile(file, bytes, size, &written, 0) != FALSE &&
      written == size;
   CloseHandle(file);
   return result;
}

bool FileHasSize(const char* path, const DWORD expected)
{
   HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
   if (file == INVALID_HANDLE_VALUE)
      return false;

   DWORD size = 0;
   const bool result = NewSpellsMap::GetFileSize32(file, size) &&
      size == expected;
   CloseHandle(file);
   return result;
}

} // namespace

int main(int argc, char** argv)
{
   if (!AiInteropContractIsUsable())
      return 20;
   if (!ProviderInteropContractIsUsable())
      return 22;

   if (argc >= 3 && std::strcmp(argv[1], "--set") == 0)
   {
      NewSpellsMap::State state;
      NewSpellsMap::Clear(state);
      for (int i = 3; i < argc; ++i)
      {
         const int spellId = std::atoi(argv[i]);
         if (!NewSpellsMap::IsPlayableSpell(spellId))
            return 10;
         NewSpellsMap::SetDisabled(state, spellId, true);
      }
      return NewSpellsMap::RewriteTrailerFileA(argv[2], state) ? 0 : 11;
   }

   if (argc == 3 && std::strcmp(argv[1], "--clear") == 0)
   {
      NewSpellsMap::State state;
      NewSpellsMap::Clear(state);
      return NewSpellsMap::RewriteTrailerFileA(argv[2], state) ? 0 : 12;
   }

   if (argc != 2)
      return 1;

   const unsigned char base[] =
      {0x1F,0x8B,0x08,0x00,0x4E,0x53,0x4D,0x31,0x00,0x03,0x03,0x00};
   if (!WriteBaseFile(argv[1], base, sizeof(base)))
      return 2;

   NewSpellsMap::State state;
   NewSpellsMap::Clear(state);
   if (!NewSpellsMap::RewriteTrailerFileA(argv[1], state) ||
       !FileHasSize(argv[1], sizeof(base)))
      return 3;

   NewSpellsMap::SetDisabled(state, 71, true);
   NewSpellsMap::SetDisabled(state, 95, true);
   NewSpellsMap::SetDisabled(state, 96, true);
   if (!NewSpellsMap::RewriteTrailerFileA(argv[1], state) ||
       !FileHasSize(argv[1], sizeof(base) + sizeof(NewSpellsMap::Trailer)))
      return 4;

   NewSpellsMap::State loaded;
   if (!NewSpellsMap::ReadTrailerFileA(argv[1], loaded) ||
       !NewSpellsMap::Equals(state, loaded) ||
       !NewSpellsMap::IsDisabled(loaded, 71) ||
       !NewSpellsMap::IsDisabled(loaded, 95) ||
       !NewSpellsMap::IsDisabled(loaded, 96) ||
       NewSpellsMap::IsDisabled(loaded, 94))
      return 5;

   NewSpellsMap::SetDisabled(state, 95, false);
   if (!NewSpellsMap::RewriteTrailerFileA(argv[1], state) ||
       !FileHasSize(argv[1], sizeof(base) + sizeof(NewSpellsMap::Trailer)) ||
       !NewSpellsMap::ReadTrailerFileA(argv[1], loaded) ||
       !NewSpellsMap::Equals(state, loaded))
      return 6;

   NewSpellsMap::Clear(state);
   if (!NewSpellsMap::RewriteTrailerFileA(argv[1], state) ||
       !FileHasSize(argv[1], sizeof(base)) ||
       NewSpellsMap::ReadTrailerFileA(argv[1], loaded))
      return 7;

   NewSpellsMap::Trailer trailer;
   NewSpellsMap::BuildTrailer(state, trailer);
   if (!NewSpellsMap::ValidateTrailer(trailer))
      return 8;
   ++trailer.checksum;
   if (NewSpellsMap::ValidateTrailer(trailer))
      return 9;

   DeleteFileA(argv[1]);
   return 0;
}
