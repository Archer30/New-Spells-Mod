#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace NewSpellsMap
{

const std::uint32_t FORMAT_VERSION = 1;
const std::uint32_t SPELL_BIT_COUNT = 128;
const std::size_t DISABLED_BYTE_COUNT = SPELL_BIT_COUNT / 8;

// These are the spells implemented by the New Spells core.  External spell
// providers use the reserved range below and are discovered at runtime by the
// game and map-editor plugins.
const int BUILTIN_SPELL_IDS[] =
{
   71, 73, 75, 81, 82, 83, 84, 85, 86, 87,
   88, 89, 90, 91, 92, 93, 94, 95
};

const std::size_t BUILTIN_SPELL_COUNT =
   sizeof(BUILTIN_SPELL_IDS) / sizeof(BUILTIN_SPELL_IDS[0]);

const int EXTERNAL_SPELL_ID_FIRST = 96;
const int EXTERNAL_SPELL_ID_LAST = 126;
const std::size_t EXTERNAL_SPELL_SLOT_COUNT =
   EXTERNAL_SPELL_ID_LAST - EXTERNAL_SPELL_ID_FIRST + 1;

// Compatibility aliases for code which iterates the core-owned spells.  The
// dynamic editor catalog deliberately does not use these aliases for external
// providers.
static const int (&PLAYABLE_SPELL_IDS)[BUILTIN_SPELL_COUNT] =
   BUILTIN_SPELL_IDS;
const std::size_t PLAYABLE_SPELL_COUNT = BUILTIN_SPELL_COUNT;

const unsigned char TRAILER_MAGIC[12] =
{
   'N', 'S', 'M', 'A', 'P', 'S', 'P', 'E', 'L', 'L', 'S', '1'
};

#pragma pack(push, 1)
struct State
{
   unsigned char disabled[DISABLED_BYTE_COUNT];
};

struct Trailer
{
   unsigned char magic[sizeof(TRAILER_MAGIC)];
   std::uint32_t version;
   std::uint32_t bitCount;
   unsigned char disabled[DISABLED_BYTE_COUNT];
   std::uint32_t checksum;
   std::uint32_t totalSize;
};
#pragma pack(pop)

static_assert(sizeof(State) == 16, "New Spells map state layout changed");
static_assert(sizeof(Trailer) == 44, "New Spells map trailer layout changed");

inline void Clear(State& state)
{
   std::memset(&state, 0, sizeof(state));
}

inline bool Equals(const State& left, const State& right)
{
   return std::memcmp(&left, &right, sizeof(State)) == 0;
}

inline bool IsBuiltinSpell(const int spellId)
{
   for (std::size_t i = 0; i < BUILTIN_SPELL_COUNT; ++i)
      if (BUILTIN_SPELL_IDS[i] == spellId)
         return true;

   return false;
}

inline bool IsExternalSpellSlot(const int spellId)
{
   return spellId >= EXTERNAL_SPELL_ID_FIRST &&
      spellId <= EXTERNAL_SPELL_ID_LAST;
}

inline bool IsPlayableSpell(const int spellId)
{
   return IsBuiltinSpell(spellId) || IsExternalSpellSlot(spellId);
}

inline bool IsDisabled(const State& state, const int spellId)
{
   return spellId >= 0 && spellId < static_cast<int>(SPELL_BIT_COUNT) &&
      (state.disabled[spellId / 8] & (1u << (spellId % 8))) != 0;
}

inline void SetDisabled(State& state, const int spellId, const bool disabled)
{
   if (spellId < 0 || spellId >= static_cast<int>(SPELL_BIT_COUNT))
      return;

   const unsigned char mask = static_cast<unsigned char>(1u << (spellId % 8));
   if (disabled)
      state.disabled[spellId / 8] |= mask;
   else
      state.disabled[spellId / 8] &= static_cast<unsigned char>(~mask);
}

inline bool AnyDisabled(const State& state)
{
   // The trailer is a 128-bit shared persistence record.  Bits belonging to
   // providers which are not currently installed must survive a map-editor
   // save, so deciding whether a trailer is needed cannot be limited to the
   // core-owned spell list.
   for (std::size_t i = 0; i < DISABLED_BYTE_COUNT; ++i)
      if (state.disabled[i] != 0)
         return true;

   return false;
}

inline std::uint32_t UpdateChecksum(std::uint32_t checksum,
                                    const void* data,
                                    const std::size_t size)
{
   const unsigned char* bytes = static_cast<const unsigned char*>(data);
   for (std::size_t i = 0; i < size; ++i)
   {
      checksum ^= bytes[i];
      checksum *= 16777619u;
   }
   return checksum;
}

inline std::uint32_t ComputeChecksum(const Trailer& trailer)
{
   std::uint32_t checksum = 2166136261u;
   checksum = UpdateChecksum(checksum, &trailer,
      offsetof(Trailer, checksum));
   return UpdateChecksum(checksum, &trailer.totalSize,
      sizeof(trailer.totalSize));
}

inline void BuildTrailer(const State& state, Trailer& trailer)
{
   std::memset(&trailer, 0, sizeof(trailer));
   std::memcpy(trailer.magic, TRAILER_MAGIC, sizeof(TRAILER_MAGIC));
   trailer.version = FORMAT_VERSION;
   trailer.bitCount = SPELL_BIT_COUNT;
   std::memcpy(trailer.disabled, state.disabled, sizeof(state.disabled));
   trailer.totalSize = sizeof(Trailer);
   trailer.checksum = ComputeChecksum(trailer);
}

inline bool ValidateTrailer(const Trailer& trailer)
{
   return std::memcmp(trailer.magic, TRAILER_MAGIC,
                      sizeof(TRAILER_MAGIC)) == 0 &&
      trailer.version == FORMAT_VERSION &&
      trailer.bitCount == SPELL_BIT_COUNT &&
      trailer.totalSize == sizeof(Trailer) &&
      trailer.checksum == ComputeChecksum(trailer);
}

inline bool GetFileSize32(HANDLE file, DWORD& size)
{
   DWORD high = 0;
   const DWORD low = GetFileSize(file, &high);
   if (low == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
      return false;
   if (high != 0)
      return false;

   size = low;
   return true;
}

inline bool ReadTrailerFromHandle(HANDLE file, State& state,
                                  DWORD& contentSize)
{
   Clear(state);

   DWORD fileSize = 0;
   if (!GetFileSize32(file, fileSize))
      return false;

   contentSize = fileSize;
   if (fileSize < sizeof(Trailer))
      return false;

   SetLastError(NO_ERROR);
   if (SetFilePointer(file, fileSize - sizeof(Trailer), 0, FILE_BEGIN) ==
       INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
      return false;

   Trailer trailer;
   DWORD bytesRead = 0;
   if (!ReadFile(file, &trailer, sizeof(trailer), &bytesRead, 0) ||
       bytesRead != sizeof(trailer) || !ValidateTrailer(trailer))
      return false;

   std::memcpy(state.disabled, trailer.disabled, sizeof(state.disabled));
   contentSize = fileSize - sizeof(Trailer);
   return true;
}

inline bool ReadTrailerFileA(const char* path, State& state)
{
   Clear(state);
   if (!path || !*path)
      return false;

   HANDLE file = CreateFileA(path, GENERIC_READ,
      FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL, 0);
   if (file == INVALID_HANDLE_VALUE)
      return false;

   DWORD contentSize = 0;
   const bool result = ReadTrailerFromHandle(file, state, contentSize);
   CloseHandle(file);
   return result;
}

inline bool RewriteTrailerFileA(const char* path, const State& state)
{
   if (!path || !*path)
      return false;

   HANDLE file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
   if (file == INVALID_HANDLE_VALUE)
      return false;

   DWORD fileSize = 0;
   DWORD contentSize = 0;
   State previousState;
   bool success = GetFileSize32(file, fileSize);
   if (success)
   {
      contentSize = fileSize;
      ReadTrailerFromHandle(file, previousState, contentSize);

      SetLastError(NO_ERROR);
      success = SetFilePointer(file, contentSize, 0, FILE_BEGIN) !=
         INVALID_SET_FILE_POINTER || GetLastError() == NO_ERROR;
   }

   if (success)
      success = SetEndOfFile(file) != FALSE;

   if (success && AnyDisabled(state))
   {
      Trailer trailer;
      BuildTrailer(state, trailer);
      DWORD bytesWritten = 0;
      success = WriteFile(file, &trailer, sizeof(trailer),
                          &bytesWritten, 0) != FALSE &&
         bytesWritten == sizeof(trailer);
   }

   if (success)
      success = FlushFileBuffers(file) != FALSE;

   CloseHandle(file);
   return success;
}

} // namespace NewSpellsMap
