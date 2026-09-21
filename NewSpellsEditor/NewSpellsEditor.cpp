#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "..\NewSpells\NewSpellsMapFormat.h"
#include "..\NewSpells\NewSpellsProviderApi.h"
#include "..\NewSpells\patcher_x86_commented.hpp"

namespace
{

const std::uintptr_t PREFERRED_IMAGE_BASE = 0x400000;
const int SPELL_CHECKLIST_ID = 1937;
const std::size_t CHECKLIST_OBJECT_OFFSET = 184;
const std::size_t CHECKLIST_HWND_OFFSET = 212;

HMODULE g_module = 0;
HMODULE g_editor = 0;
Patcher* g_patcher = 0;
PatcherInstance* g_instance = 0;
bool g_active = false;
void* g_currentDocument = 0;
NewSpellsMap::State g_disabledSpells = {};

struct SpellCatalogEntry
{
   int spellId;
   std::string name;
};

struct ExternalSpellCandidate
{
   int spellId;
   std::string utf8Name;
   bool editorVisible;
};

std::vector<SpellCatalogEntry> g_spellCatalog;
bool g_spellCatalogLoaded = false;

std::uintptr_t Address(const std::uintptr_t preferredAddress)
{
   return reinterpret_cast<std::uintptr_t>(g_editor) +
      (preferredAddress - PREFERRED_IMAGE_BASE);
}

template<std::size_t Size>
bool MatchesCode(const std::uintptr_t preferredAddress,
                 const unsigned char (&expected)[Size])
{
   return std::memcmp(reinterpret_cast<const void*>(Address(preferredAddress)),
                      expected, Size) == 0;
}

bool HasOnlyAppliedHiHooks(const std::uintptr_t preferredAddress)
{
   Patch* patch = g_patcher->GetFirstPatchAt(Address(preferredAddress));
   if (!patch)
      return false;

   for (; patch; patch = patch->GetAppliedAfter())
   {
      if (patch->GetAddress() != Address(preferredAddress) ||
          !patch->IsApplied() || patch->GetType() != HIHOOK_)
         return false;
   }

   return true;
}

template<std::size_t Size>
bool IsHookable(const std::uintptr_t preferredAddress,
                const unsigned char (&expected)[Size])
{
   return MatchesCode(preferredAddress, expected) ||
      HasOnlyAppliedHiHooks(preferredAddress);
}

bool HasExpectedVtableEntry(const std::uintptr_t entryAddress,
                            const std::uintptr_t functionAddress)
{
   return *reinterpret_cast<const std::uintptr_t*>(Address(entryAddress)) ==
      Address(functionAddress);
}

bool ValidateEditorImage()
{
   g_editor = GetModuleHandleA(0);
   if (!g_editor)
      return false;

   const IMAGE_DOS_HEADER* dos =
      reinterpret_cast<const IMAGE_DOS_HEADER*>(g_editor);
   if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
      return false;

   const IMAGE_NT_HEADERS32* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
      reinterpret_cast<const unsigned char*>(g_editor) + dos->e_lfanew);
   if (nt->Signature != IMAGE_NT_SIGNATURE ||
       nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
       nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
       nt->OptionalHeader.ImageBase != PREFERRED_IMAGE_BASE ||
       nt->OptionalHeader.SizeOfImage <=
          (0x53C3E8 - PREFERRED_IMAGE_BASE + sizeof(std::uintptr_t)))
      return false;

   const unsigned char onInit[] =
      {0x55,0x8B,0xEC,0x83,0xEC,0x20,0xA1,0x4C,0x13,0x5A,0x00,0x53,0x56,0x57,0xFF,0x30,0x8B,0xD9,0x68,0x92,0x07,0x00,0x00,0xE8};
   const unsigned char onApply[] =
      {0x56,0x8B,0xF1,0xE8,0x26,0xC0,0x09,0x00,0x8B,0x8E,0x90,0x00,0x00,0x00,0x8D,0x86,0x98,0x00,0x00,0x00};
   const unsigned char onCheckChange[] =
      {0x55,0x8B,0xEC,0x83,0xEC,0x1C,0x53,0x56,0x57,0x8B,0x1D,0x3C,0x06,0x53,0x00,0x33,0xFF,0x8B,0xF1,0x57,0x57,0x68,0x88,0x01};
   const unsigned char onNewDocument[] =
      {0xB8,0xDE,0x66,0x52,0x00,0xE8,0x29,0x77,0x08,0x00,0x81,0xEC,0x6C,0x01,0x00,0x00,0x53,0x56,0x8B,0xF1,0x33,0xDB,0x38,0x9E};
   const unsigned char onOpenDocument[] =
      {0x56,0x8B,0xF1,0xFF,0x74,0x24,0x08,0xE8,0x80,0xB5,0x0A,0x00,0x85,0xC0,0x74,0x0F,0xFF,0x15,0x2C,0x02,0x53,0x00,0x6A,0x01};
   const unsigned char onSaveDocument[] =
      {0x56,0x8B,0xF1,0xFF,0x74,0x24,0x08,0xE8,0x27,0xB7,0x0A,0x00,0x85,0xC0,0x74,0x0F,0xFF,0x15,0x2C,0x02,0x53,0x00,0x6A,0x01};
   const unsigned char setCheck[] =
      {0x53,0x8B,0x5C,0x24,0x0C,0x56,0x83,0xFB,0x02,0x8B,0xF1,0x75,0x0C,0x8B,0x46,0x40,0x3B,0xC3,0x74,0x62};
   const unsigned char getCheck[] =
      {0x8B,0x01,0x6A,0x00,0xFF,0x74,0x24,0x08,0x68,0x99,0x01,0x00,0x00,0xFF,0x90,0xA0,0x00,0x00,0x00,0x83};
   const unsigned char setModified[] =
      {0x8B,0x44,0x24,0x04,0x89,0x41,0x44,0xC2,0x04,0x00};

   return HasExpectedVtableEntry(0x53C3E0, 0x47666E) &&
      HasExpectedVtableEntry(0x53C3E8, 0x47680C) &&
      HasExpectedVtableEntry(0x53C304, 0x476860) &&
      HasExpectedVtableEntry(0x53A864, 0x45EF8D) &&
      HasExpectedVtableEntry(0x53A868, 0x45F3BB) &&
      HasExpectedVtableEntry(0x53A86C, 0x45F3DE) &&
      IsHookable(0x47666E, onInit) &&
      IsHookable(0x47680C, onApply) &&
      IsHookable(0x476860, onCheckChange) &&
      IsHookable(0x45EF8D, onNewDocument) &&
      IsHookable(0x45F3BB, onOpenDocument) &&
      IsHookable(0x45F3DE, onSaveDocument) &&
      MatchesCode(0x515D35, setCheck) &&
      MatchesCode(0x515DB0, getCheck) &&
      MatchesCode(0x45ECE4, setModified);
}

bool ReadFileText(const char* path, std::string& text)
{
   HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0,
      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
   if (file == INVALID_HANDLE_VALUE)
      return false;

   DWORD size = 0;
   bool success = NewSpellsMap::GetFileSize32(file, size) && size <= 1024 * 1024;
   if (success)
   {
      std::vector<char> buffer(size + 1, 0);
      DWORD bytesRead = 0;
      success = ReadFile(file, &buffer[0], size, &bytesRead, 0) != FALSE &&
         bytesRead == size;
      if (success)
         text.assign(&buffer[0], size);
   }

   CloseHandle(file);
   return success;
}

void SkipJsonWhitespace(const std::string& text, std::size_t& position,
                        const std::size_t end)
{
   while (position < end &&
          std::isspace(static_cast<unsigned char>(text[position])))
      ++position;
}

int HexDigit(const char value)
{
   if (value >= '0' && value <= '9')
      return value - '0';
   if (value >= 'a' && value <= 'f')
      return value - 'a' + 10;
   if (value >= 'A' && value <= 'F')
      return value - 'A' + 10;
   return -1;
}

bool ParseHexCodeUnit(const std::string& text, std::size_t& position,
                      const std::size_t end, std::uint32_t& codeUnit)
{
   if (end - position < 4)
      return false;

   codeUnit = 0;
   for (int i = 0; i < 4; ++i)
   {
      const int digit = HexDigit(text[position++]);
      if (digit < 0)
         return false;
      codeUnit = codeUnit * 16 + static_cast<std::uint32_t>(digit);
   }
   return true;
}

bool AppendUtf8(std::string& value, const std::uint32_t codePoint)
{
   if (codePoint == 0 || codePoint > 0x10FFFF ||
       (codePoint >= 0xD800 && codePoint <= 0xDFFF))
      return false;

   if (codePoint <= 0x7F)
      value.push_back(static_cast<char>(codePoint));
   else if (codePoint <= 0x7FF)
   {
      value.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
      value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
   }
   else if (codePoint <= 0xFFFF)
   {
      value.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
      value.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
      value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
   }
   else
   {
      value.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
      value.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
      value.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
      value.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
   }
   return true;
}

bool ParseJsonString(const std::string& text, std::size_t& position,
                     const std::size_t end, std::string* value)
{
   if (position >= end || end > text.size() || text[position++] != '"')
      return false;

   if (value)
      value->clear();

   while (position < end)
   {
      const unsigned char ch = static_cast<unsigned char>(text[position++]);
      if (ch == '"')
         return true;
      if (ch < 0x20)
         return false;
      if (ch != '\\')
      {
         if (value)
            value->push_back(static_cast<char>(ch));
         continue;
      }

      if (position >= end)
         return false;
      const char escaped = text[position++];
      char decoded = 0;
      switch (escaped)
      {
      case '"': decoded = '"'; break;
      case '\\': decoded = '\\'; break;
      case '/': decoded = '/'; break;
      case 'b': decoded = '\b'; break;
      case 'f': decoded = '\f'; break;
      case 'n': decoded = '\n'; break;
      case 'r': decoded = '\r'; break;
      case 't': decoded = '\t'; break;
      case 'u':
      {
         std::uint32_t codePoint = 0;
         if (!ParseHexCodeUnit(text, position, end, codePoint))
            return false;
         if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
         {
            if (end - position < 6 || text[position] != '\\' ||
                text[position + 1] != 'u')
               return false;
            position += 2;
            std::uint32_t low = 0;
            if (!ParseHexCodeUnit(text, position, end, low) ||
                low < 0xDC00 || low > 0xDFFF)
               return false;
            codePoint = 0x10000 + ((codePoint - 0xD800) << 10) +
               (low - 0xDC00);
         }
         else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
            return false;

         if (value && !AppendUtf8(*value, codePoint))
            return false;
         break;
      }
      default:
         return false;
      }

      if (escaped != 'u' && value)
         value->push_back(decoded);
   }

   return false;
}

bool SkipJsonValue(const std::string& text, std::size_t& position,
                   const std::size_t end, const int depth)
{
   if (depth > 64)
      return false;

   SkipJsonWhitespace(text, position, end);
   if (position >= end)
      return false;

   if (text[position] == '"')
      return ParseJsonString(text, position, end, 0);

   if (text[position] == '{')
   {
      ++position;
      SkipJsonWhitespace(text, position, end);
      if (position < end && text[position] == '}')
      {
         ++position;
         return true;
      }

      while (position < end)
      {
         if (!ParseJsonString(text, position, end, 0))
            return false;
         SkipJsonWhitespace(text, position, end);
         if (position >= end || text[position++] != ':')
            return false;
         if (!SkipJsonValue(text, position, end, depth + 1))
            return false;
         SkipJsonWhitespace(text, position, end);
         if (position >= end)
            return false;
         if (text[position] == '}')
         {
            ++position;
            return true;
         }
         if (text[position++] != ',')
            return false;
         SkipJsonWhitespace(text, position, end);
      }
      return false;
   }

   if (text[position] == '[')
   {
      ++position;
      SkipJsonWhitespace(text, position, end);
      if (position < end && text[position] == ']')
      {
         ++position;
         return true;
      }

      while (position < end)
      {
         if (!SkipJsonValue(text, position, end, depth + 1))
            return false;
         SkipJsonWhitespace(text, position, end);
         if (position >= end)
            return false;
         if (text[position] == ']')
         {
            ++position;
            return true;
         }
         if (text[position++] != ',')
            return false;
         SkipJsonWhitespace(text, position, end);
      }
      return false;
   }

   const char* literals[] = {"true", "false", "null"};
   for (std::size_t i = 0; i < sizeof(literals) / sizeof(literals[0]); ++i)
   {
      const std::size_t length = std::strlen(literals[i]);
      if (end - position >= length &&
          text.compare(position, length, literals[i]) == 0)
      {
         position += length;
         return true;
      }
   }

   const std::size_t numberBegin = position;
   if (text[position] == '-')
      ++position;
   if (position >= end)
      return false;
   if (text[position] == '0')
      ++position;
   else
   {
      if (text[position] < '1' || text[position] > '9')
         return false;
      while (position < end && text[position] >= '0' && text[position] <= '9')
         ++position;
   }
   if (position < end && text[position] == '.')
   {
      ++position;
      const std::size_t fractionBegin = position;
      while (position < end && text[position] >= '0' && text[position] <= '9')
         ++position;
      if (position == fractionBegin)
         return false;
   }
   if (position < end && (text[position] == 'e' || text[position] == 'E'))
   {
      ++position;
      if (position < end && (text[position] == '+' || text[position] == '-'))
         ++position;
      const std::size_t exponentBegin = position;
      while (position < end && text[position] >= '0' && text[position] <= '9')
         ++position;
      if (position == exponentBegin)
         return false;
   }
   return position > numberBegin;
}

bool ParseJsonDocument(const std::string& text, std::size_t& objectBegin,
                       std::size_t& objectEnd)
{
   std::size_t position = 0;
   if (text.size() >= 3 &&
       static_cast<unsigned char>(text[0]) == 0xEF &&
       static_cast<unsigned char>(text[1]) == 0xBB &&
       static_cast<unsigned char>(text[2]) == 0xBF)
      position = 3;

   SkipJsonWhitespace(text, position, text.size());
   if (position >= text.size() || text[position] != '{')
      return false;

   objectBegin = position;
   if (!SkipJsonValue(text, position, text.size(), 0))
      return false;
   objectEnd = position;
   SkipJsonWhitespace(text, position, text.size());
   return position == text.size();
}

enum JsonMemberResult
{
   JSON_MEMBER_INVALID = -1,
   JSON_MEMBER_ABSENT = 0,
   JSON_MEMBER_FOUND = 1,
   JSON_MEMBER_DUPLICATE = 2
};

JsonMemberResult FindJsonMember(const std::string& text,
                                const std::size_t objectBegin,
                                const std::size_t objectEnd,
                                const char* key,
                                std::size_t& valueBegin,
                                std::size_t& valueEnd)
{
   if (!key || objectBegin >= objectEnd || objectEnd > text.size() ||
       text[objectBegin] != '{' || text[objectEnd - 1] != '}')
      return JSON_MEMBER_INVALID;

   int matches = 0;
   std::size_t position = objectBegin + 1;
   SkipJsonWhitespace(text, position, objectEnd - 1);
   if (position == objectEnd - 1)
      return JSON_MEMBER_ABSENT;

   while (position < objectEnd - 1)
   {
      std::string memberName;
      if (!ParseJsonString(text, position, objectEnd - 1, &memberName))
         return JSON_MEMBER_INVALID;
      SkipJsonWhitespace(text, position, objectEnd - 1);
      if (position >= objectEnd - 1 || text[position++] != ':')
         return JSON_MEMBER_INVALID;
      SkipJsonWhitespace(text, position, objectEnd - 1);

      const std::size_t currentBegin = position;
      if (!SkipJsonValue(text, position, objectEnd - 1, 1))
         return JSON_MEMBER_INVALID;
      const std::size_t currentEnd = position;
      if (memberName == key)
      {
         if (++matches == 1)
         {
            valueBegin = currentBegin;
            valueEnd = currentEnd;
         }
      }

      SkipJsonWhitespace(text, position, objectEnd - 1);
      if (position == objectEnd - 1)
         break;
      if (text[position++] != ',')
         return JSON_MEMBER_INVALID;
      SkipJsonWhitespace(text, position, objectEnd - 1);
   }

   if (matches == 0)
      return JSON_MEMBER_ABSENT;
   return matches == 1 ? JSON_MEMBER_FOUND : JSON_MEMBER_DUPLICATE;
}

bool FindJsonObject(const std::string& text,
                    const std::size_t parentBegin,
                    const std::size_t parentEnd, const char* key,
                    std::size_t& objectBegin, std::size_t& objectEnd)
{
   std::size_t valueBegin = 0;
   std::size_t valueEnd = 0;
   if (FindJsonMember(text, parentBegin, parentEnd, key,
                      valueBegin, valueEnd) != JSON_MEMBER_FOUND ||
       valueBegin >= valueEnd || text[valueBegin] != '{')
      return false;

   objectBegin = valueBegin;
   objectEnd = valueEnd;
   return true;
}

bool FindJsonStringProperty(const std::string& text,
                            const std::size_t objectBegin,
                            const std::size_t objectEnd, const char* key,
                            std::string& value)
{
   std::size_t valueBegin = 0;
   std::size_t valueEnd = 0;
   if (FindJsonMember(text, objectBegin, objectEnd, key,
                      valueBegin, valueEnd) != JSON_MEMBER_FOUND)
      return false;

   std::size_t position = valueBegin;
   return ParseJsonString(text, position, valueEnd, &value) &&
      position == valueEnd && !value.empty();
}

bool FindJsonStringPropertyAllowEmpty(const std::string& text,
                                      const std::size_t objectBegin,
                                      const std::size_t objectEnd,
                                      const char* key,
                                      std::string& value)
{
   std::size_t valueBegin = 0;
   std::size_t valueEnd = 0;
   if (FindJsonMember(text, objectBegin, objectEnd, key,
                      valueBegin, valueEnd) != JSON_MEMBER_FOUND)
      return false;

   std::size_t position = valueBegin;
   return ParseJsonString(text, position, valueEnd, &value) &&
      position == valueEnd;
}

bool ParseUnsignedDecimal(const std::string& text, std::uint32_t& value)
{
   if (text.empty())
      return false;

   std::uint32_t result = 0;
   for (std::size_t i = 0; i < text.size(); ++i)
   {
      if (text[i] < '0' || text[i] > '9')
         return false;
      const std::uint32_t digit = static_cast<std::uint32_t>(text[i] - '0');
      if (result > (0xFFFFFFFFu - digit) / 10u)
         return false;
      result = result * 10u + digit;
   }
   value = result;
   return true;
}

bool ParseSignedDecimal(const std::string& text, int& value)
{
   if (text.empty())
      return false;

   std::size_t position = 0;
   const bool negative = text[position] == '-';
   if (negative && ++position == text.size())
      return false;

   const std::uint32_t limit = negative ? 0x80000000u : 0x7FFFFFFFu;
   std::uint32_t result = 0;
   for (; position < text.size(); ++position)
   {
      if (text[position] < '0' || text[position] > '9')
         return false;
      const std::uint32_t digit = static_cast<std::uint32_t>(
         text[position] - '0');
      if (result > (limit - digit) / 10u)
         return false;
      result = result * 10u + digit;
   }

   if (negative)
      value = result == 0x80000000u ? INT_MIN : -static_cast<int>(result);
   else
      value = static_cast<int>(result);
   return true;
}

bool FindJsonUnsignedProperty(const std::string& text,
                              const std::size_t objectBegin,
                              const std::size_t objectEnd, const char* key,
                              std::uint32_t& value)
{
   std::size_t valueBegin = 0;
   std::size_t valueEnd = 0;
   if (FindJsonMember(text, objectBegin, objectEnd, key,
                      valueBegin, valueEnd) != JSON_MEMBER_FOUND)
      return false;

   std::string decimal;
   if (valueBegin < valueEnd && text[valueBegin] == '"')
   {
      std::size_t position = valueBegin;
      if (!ParseJsonString(text, position, valueEnd, &decimal) ||
          position != valueEnd)
         return false;
   }
   else
      decimal.assign(text, valueBegin, valueEnd - valueBegin);
   return ParseUnsignedDecimal(decimal, value);
}

bool FindJsonSignedProperty(const std::string& text,
                            const std::size_t objectBegin,
                            const std::size_t objectEnd, const char* key,
                            int& value)
{
   std::size_t valueBegin = 0;
   std::size_t valueEnd = 0;
   if (FindJsonMember(text, objectBegin, objectEnd, key,
                      valueBegin, valueEnd) != JSON_MEMBER_FOUND)
      return false;

   std::string decimal;
   if (valueBegin < valueEnd && text[valueBegin] == '"')
   {
      std::size_t position = valueBegin;
      if (!ParseJsonString(text, position, valueEnd, &decimal) ||
          position != valueEnd)
         return false;
   }
   else
      decimal.assign(text, valueBegin, valueEnd - valueBegin);
   return ParseSignedDecimal(decimal, value);
}

bool FindOptionalJsonStringProperty(const std::string& text,
                                    const std::size_t objectBegin,
                                    const std::size_t objectEnd,
                                    const char* key, bool& present,
                                    std::string& value)
{
   std::size_t valueBegin = 0;
   std::size_t valueEnd = 0;
   const JsonMemberResult result = FindJsonMember(text, objectBegin,
      objectEnd, key, valueBegin, valueEnd);
   if (result == JSON_MEMBER_ABSENT)
   {
      present = false;
      value.clear();
      return true;
   }
   if (result != JSON_MEMBER_FOUND)
      return false;

   present = true;
   std::size_t position = valueBegin;
   return ParseJsonString(text, position, valueEnd, &value) &&
      position == valueEnd;
}

bool FindOptionalJsonUnsignedProperty(const std::string& text,
                                      const std::size_t objectBegin,
                                      const std::size_t objectEnd,
                                      const char* key, bool& present,
                                      std::uint32_t& value)
{
   std::size_t valueBegin = 0;
   std::size_t valueEnd = 0;
   const JsonMemberResult result = FindJsonMember(text, objectBegin,
      objectEnd, key, valueBegin, valueEnd);
   if (result == JSON_MEMBER_ABSENT)
   {
      present = false;
      value = 0;
      return true;
   }
   if (result != JSON_MEMBER_FOUND)
      return false;

   present = true;
   return FindJsonUnsignedProperty(text, objectBegin, objectEnd, key, value);
}

bool FindStandardSpellObject(const std::string& text,
                             const std::size_t rootBegin,
                             const std::size_t rootEnd,
                             const int spellId,
                             std::size_t& spellBegin,
                             std::size_t& spellEnd)
{
   char spellKey[16] = {};
   if (sprintf_s(spellKey, sizeof(spellKey), "%d", spellId) < 0)
      return false;

   std::size_t eraBegin = 0;
   std::size_t eraEnd = 0;
   std::size_t spellsBegin = 0;
   std::size_t spellsEnd = 0;
   return FindJsonObject(text, rootBegin, rootEnd,
                         "era", eraBegin, eraEnd) &&
      FindJsonObject(text, eraBegin, eraEnd, "spells",
                     spellsBegin, spellsEnd) &&
      FindJsonObject(text, spellsBegin, spellsEnd, spellKey,
                     spellBegin, spellEnd);
}

bool ValidateStandardSpellRecord(const std::string& text,
                                 const std::size_t rootBegin,
                                 const std::size_t rootEnd,
                                 const int spellId,
                                 std::string& name,
                                 std::uint32_t& flags)
{
   const std::uint32_t KNOWN_SPELL_FLAGS = 0x001FFFFFu;
   std::size_t spellBegin = 0;
   std::size_t spellEnd = 0;
   if (!FindStandardSpellObject(text, rootBegin, rootEnd, spellId,
                                spellBegin, spellEnd))
      return false;

   int type = 0;
   int spellEffect = 0;
   std::uint32_t animationIndex = 0;
   std::uint32_t level = 0;
   std::uint32_t school = 0;
   std::string soundName;
   std::string shortName;
   if (!FindJsonSignedProperty(text, spellBegin, spellEnd, "type", type) ||
       type < -128 || type > 127 ||
       !FindJsonStringPropertyAllowEmpty(text, spellBegin, spellEnd,
                                         "soundName", soundName) ||
       !FindJsonUnsignedProperty(text, spellBegin, spellEnd,
                                 "animationIndex", animationIndex) ||
       // IDs 83+ belong to the relocated New Spells/custom animation block and
       // must be resolved by a stable namespaced animation key.
       animationIndex >= 83 ||
       !FindJsonUnsignedProperty(text, spellBegin, spellEnd,
                                 "flags", flags) ||
       (flags & ~KNOWN_SPELL_FLAGS) != 0 ||
       (flags & 0x00100000u) != 0 ||
       !FindJsonStringProperty(text, spellBegin, spellEnd, "name", name) ||
       !FindJsonStringProperty(text, spellBegin, spellEnd,
                               "shortName", shortName) ||
       !FindJsonUnsignedProperty(text, spellBegin, spellEnd,
                                 "level", level) ||
       level < 1 || level > 5 ||
       !FindJsonUnsignedProperty(text, spellBegin, spellEnd,
                                 "school", school) ||
       (school & ~0xFu) != 0 ||
       !FindJsonSignedProperty(text, spellBegin, spellEnd,
                               "spEffect", spellEffect) ||
       spellEffect < -1000000 || spellEffect > 1000000)
      return false;

   std::size_t manaBegin = 0;
   std::size_t manaEnd = 0;
   std::size_t effectBegin = 0;
   std::size_t effectEnd = 0;
   std::size_t aiBegin = 0;
   std::size_t aiEnd = 0;
   std::size_t descriptionBegin = 0;
   std::size_t descriptionEnd = 0;
   if (!FindJsonObject(text, spellBegin, spellEnd, "manaCost",
                       manaBegin, manaEnd) ||
       !FindJsonObject(text, spellBegin, spellEnd, "baseValue",
                       effectBegin, effectEnd) ||
       !FindJsonObject(text, spellBegin, spellEnd, "aiValue",
                       aiBegin, aiEnd) ||
       !FindJsonObject(text, spellBegin, spellEnd, "description",
                       descriptionBegin, descriptionEnd))
      return false;

   for (int mastery = 0; mastery <= 3; ++mastery)
   {
      char key[4] = {};
      if (sprintf_s(key, sizeof(key), "%d", mastery) < 0)
         return false;
      std::uint32_t mana = 0;
      std::uint32_t ai = 0;
      int effect = 0;
      std::string description;
      if (!FindJsonUnsignedProperty(text, manaBegin, manaEnd, key, mana) ||
          mana > 32767 ||
          !FindJsonSignedProperty(text, effectBegin, effectEnd, key, effect) ||
          effect < -1000000 || effect > 1000000 ||
          !FindJsonUnsignedProperty(text, aiBegin, aiEnd, key, ai) ||
          ai > 1000000000u ||
          !FindJsonStringProperty(text, descriptionBegin, descriptionEnd,
                                  key, description))
         return false;
   }

   std::size_t chanceBegin = 0;
   std::size_t chanceEnd = 0;
   if (!FindJsonObject(text, spellBegin, spellEnd, "chanceToGet",
                       chanceBegin, chanceEnd))
      return false;
   for (int town = 0; town <= 8; ++town)
   {
      char key[4] = {};
      std::uint32_t probability = 0;
      if (sprintf_s(key, sizeof(key), "%d", town) < 0 ||
          !FindJsonUnsignedProperty(text, chanceBegin, chanceEnd,
                                    key, probability) ||
          probability > 100)
         return false;
   }

   return true;
}

bool FindSpellName(const std::string& text,
                   const std::size_t rootBegin,
                   const std::size_t rootEnd, const int spellId,
                   std::string& value)
{
   char spellKey[16] = {};
   if (sprintf_s(spellKey, sizeof(spellKey), "%d", spellId) < 0)
      return false;

   std::size_t eraBegin = 0;
   std::size_t eraEnd = 0;
   std::size_t spellsBegin = 0;
   std::size_t spellsEnd = 0;
   std::size_t spellBegin = 0;
   std::size_t spellEnd = 0;
   return FindJsonObject(text, rootBegin, rootEnd,
                         "era", eraBegin, eraEnd) &&
      FindJsonObject(text, eraBegin, eraEnd, "spells",
                     spellsBegin, spellsEnd) &&
      FindJsonObject(text, spellsBegin, spellsEnd, spellKey,
                     spellBegin, spellEnd) &&
      FindJsonStringProperty(text, spellBegin, spellEnd, "name", value);
}

bool GetModRoot(char* path, const std::size_t capacity)
{
   const DWORD length = GetModuleFileNameA(g_module, path,
      static_cast<DWORD>(capacity));
   if (!length || length >= capacity)
      return false;

   char* separator = std::strrchr(path, '\\');
   if (!separator)
      return false;
   *separator = 0;
   separator = std::strrchr(path, '\\');
   if (!separator)
      return false;
   *separator = 0;
   return true;
}

bool GetGameRoot(char* path, const std::size_t capacity)
{
   const DWORD length = GetModuleFileNameA(g_module, path,
      static_cast<DWORD>(capacity));
   if (!length || length >= capacity)
      return false;

   char* separator = std::strrchr(path, '\\');
   if (!separator)
      return false;
   *separator = 0;

   // The editor plugin is packaged below <game>\Mods\<mod>.  Locate that
   // semantic ancestor instead of assuming a particular plugin subfolder.
   for (int depth = 0; depth < 16; ++depth)
   {
      separator = std::strrchr(path, '\\');
      if (!separator)
         return false;
      if (_stricmp(separator + 1, "Mods") == 0)
      {
         *separator = 0;
         return path[0] != 0;
      }
      *separator = 0;
   }
   return false;
}

bool MakePath(char* output, const std::size_t capacity,
              const char* root, const char* relative)
{
   return sprintf_s(output, capacity, "%s\\%s", root, relative) >= 0;
}

bool IsSafePathComponent(const std::string& value)
{
   if (value.empty() || value == "." || value == "..")
      return false;

   for (std::size_t i = 0; i < value.size(); ++i)
   {
      const unsigned char ch = static_cast<unsigned char>(value[i]);
      if (ch < 0x20 || std::strchr("\\/:*?\"<>|", ch))
         return false;
   }
   return value[value.size() - 1] != '.';
}

bool ContainsModName(const std::vector<std::string>& names,
                     const std::string& candidate)
{
   for (std::size_t i = 0; i < names.size(); ++i)
      if (_stricmp(names[i].c_str(), candidate.c_str()) == 0)
         return true;
   return false;
}

bool ReadActiveModNames(const char* gameRoot,
                        std::vector<std::string>& names)
{
   char path[MAX_PATH];
   if (!MakePath(path, sizeof(path), gameRoot, "Mods\\list.txt"))
      return false;

   std::string text;
   if (!ReadFileText(path, text))
      return false;

   names.clear();
   std::size_t position = 0;
   if (text.size() >= 3 &&
       static_cast<unsigned char>(text[0]) == 0xEF &&
       static_cast<unsigned char>(text[1]) == 0xBB &&
       static_cast<unsigned char>(text[2]) == 0xBF)
      position = 3;

   while (position <= text.size())
   {
      std::size_t rawLineEnd = text.find('\n', position);
      if (rawLineEnd == std::string::npos)
         rawLineEnd = text.size();
      std::size_t lineEnd = rawLineEnd;

      std::size_t begin = position;
      while (begin < lineEnd &&
             std::isspace(static_cast<unsigned char>(text[begin])))
         ++begin;
      while (lineEnd > begin &&
             std::isspace(static_cast<unsigned char>(text[lineEnd - 1])))
         --lineEnd;

      if (begin < lineEnd && text[begin] != '*' && text[begin] != '#' &&
          text[begin] != ';')
      {
         const std::string name(text, begin, lineEnd - begin);
         if (IsSafePathComponent(name) && !ContainsModName(names, name))
            names.push_back(name);
      }

      if (rawLineEnd == text.size())
         break;
      position = rawLineEnd + 1;
   }
   return true;
}

std::string LanguageFolder(const char* gameRoot)
{
   char language[64] = {};
   char iniPath[MAX_PATH];
   if (!MakePath(iniPath, sizeof(iniPath), gameRoot, "heroes3.ini"))
      return std::string();
   GetPrivateProfileStringA("Era", "Language", "en", language,
      sizeof(language), iniPath);
   std::string normalized(language);
   std::transform(normalized.begin(), normalized.end(), normalized.begin(),
      [](const char ch) { return static_cast<char>(
         std::tolower(static_cast<unsigned char>(ch))); });

   if (normalized.compare(0, 2, "en") == 0 ||
       normalized.find("english") != std::string::npos)
      return std::string();
   if (normalized.compare(0, 2, "ru") == 0 ||
       normalized.find("russian") != std::string::npos)
      return "ru";
   if (normalized.compare(0, 2, "cn") == 0 ||
       normalized.compare(0, 2, "zh") == 0 ||
       normalized.find("chinese") != std::string::npos)
      return "cn";

   // Other language packs may use their Era language token directly as the
   // Lang subfolder.  Keep it a single safe component before constructing a
   // path from it.
   return IsSafePathComponent(normalized) ? normalized : std::string();
}

bool Utf8ToCodePage(const std::string& utf8, const UINT codePage,
                    std::string& converted)
{
   if (utf8.empty())
      return false;

   const int wideLength = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
      static_cast<int>(utf8.size()), 0, 0);
   if (wideLength <= 0)
      return false;

   std::vector<wchar_t> wide(wideLength);
   if (MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
       static_cast<int>(utf8.size()), &wide[0], wideLength) != wideLength)
      return false;

   const int ansiLength = WideCharToMultiByte(codePage, 0, &wide[0],
      wideLength, 0, 0, 0, 0);
   if (ansiLength <= 0)
      return false;

   std::vector<char> ansi(ansiLength);
   if (WideCharToMultiByte(codePage, 0, &wide[0], wideLength,
       &ansi[0], ansiLength, 0, 0) != ansiLength)
      return false;

   converted.assign(&ansi[0], ansiLength);
   return true;
}

bool LoadJsonDocument(const char* path, std::string& text,
                      std::size_t& rootBegin, std::size_t& rootEnd)
{
   return ReadFileText(path, text) &&
      ParseJsonDocument(text, rootBegin, rootEnd);
}

bool IsTechnicalIdentity(const std::string& value,
                         const std::size_t maximumLength = 63)
{
   if (value.empty() || value.size() > maximumLength)
      return false;

   for (std::size_t i = 0; i < value.size(); ++i)
   {
      const unsigned char ch = static_cast<unsigned char>(value[i]);
      if (!std::isalnum(ch) && ch != '.' && ch != '_' && ch != '-')
         return false;
   }
   return true;
}

enum ExternalDeclarationStatus
{
   EXTERNAL_DECLARATION_INVALID,
   EXTERNAL_DECLARATION_HIDDEN,
   EXTERNAL_DECLARATION_VISIBLE
};

ExternalDeclarationStatus ValidateExternalDeclaration(
   const std::string& json, const std::size_t declarationBegin,
   const std::size_t declarationEnd, const std::uint32_t spellFlags)
{
   const std::uint32_t SPELL_FLAG_BATTLE = 0x00000001u;
   const std::uint32_t SPELL_FLAG_MAP = 0x00000002u;
   const std::uint32_t SPELL_FLAG_SINGLE_TARGET = 0x00000010u;
   const std::uint32_t SPELL_FLAG_TARGET_ANYWHERE = 0x00000080u;
   const std::uint32_t SPELL_FLAG_AI_AREA_EFFECT = 0x00010000u;
   const std::uint32_t SPELL_FLAG_AI_CREATURES = 0x00080000u;

   std::string provider;
   std::string spellKey;
   std::string kind;
   std::uint32_t capabilities = 0;
   std::uint32_t editorVisible = 0;
   if (!FindJsonStringProperty(json, declarationBegin, declarationEnd,
                               "provider", provider) ||
       !FindJsonStringProperty(json, declarationBegin, declarationEnd,
                               "spellKey", spellKey) ||
       !FindJsonStringProperty(json, declarationBegin, declarationEnd,
                               "kind", kind) ||
       !FindJsonUnsignedProperty(json, declarationBegin, declarationEnd,
                                 "editorVisible", editorVisible) ||
       !IsTechnicalIdentity(provider) || !IsTechnicalIdentity(spellKey) ||
       (kind != "adventure" && kind != "combat" && kind != "hybrid") ||
        editorVisible > 1)
      return EXTERNAL_DECLARATION_INVALID;

   if (!FindJsonUnsignedProperty(json, declarationBegin, declarationEnd,
                                 "capabilities", capabilities) ||
       capabilities == 0 || (capabilities & ~NEWSPELLS_CAP_ALL_V1) != 0)
      return EXTERNAL_DECLARATION_INVALID;

   const bool hasAdventureCast =
      (capabilities & NEWSPELLS_CAP_ADVENTURE_CAST) != 0;
   const bool hasCombatCast =
      (capabilities & NEWSPELLS_CAP_COMBAT_CAST) != 0;
   const std::uint32_t combatDomainCapabilities =
      NEWSPELLS_CAP_COMBAT_TARGET | NEWSPELLS_CAP_COMBAT_CAST |
      NEWSPELLS_CAP_STATUS_APPLY | NEWSPELLS_CAP_STATUS_ROUND |
      NEWSPELLS_CAP_STATUS_REMOVE | NEWSPELLS_CAP_CURE_DISPEL |
      NEWSPELLS_CAP_BATTLE_LIFECYCLE | NEWSPELLS_CAP_CREATURE_CAST |
      NEWSPELLS_CAP_ERM_CAST | NEWSPELLS_CAP_COMBAT_AI;
   const bool kindMatchesCapabilities =
      (kind == "adventure" && hasAdventureCast &&
       (capabilities & combatDomainCapabilities) == 0) ||
      (kind == "combat" && hasCombatCast && !hasAdventureCast) ||
      (kind == "hybrid" && hasAdventureCast && hasCombatCast);
   const bool kindMatchesFlags =
      (kind == "adventure" && (spellFlags & SPELL_FLAG_MAP) != 0 &&
       (spellFlags & SPELL_FLAG_BATTLE) == 0) ||
      (kind == "combat" && (spellFlags & SPELL_FLAG_BATTLE) != 0 &&
       (spellFlags & SPELL_FLAG_MAP) == 0) ||
      (kind == "hybrid" &&
       (spellFlags & (SPELL_FLAG_BATTLE | SPELL_FLAG_MAP)) ==
          (SPELL_FLAG_BATTLE | SPELL_FLAG_MAP));
   if (!kindMatchesCapabilities || !kindMatchesFlags ||
       ((capabilities & NEWSPELLS_CAP_COMBAT_TARGET) && !hasCombatCast) ||
       ((capabilities & NEWSPELLS_CAP_COMBAT_AI) && !hasCombatCast) ||
       ((capabilities & NEWSPELLS_CAP_ADVENTURE_AI) && !hasAdventureCast))
      return EXTERNAL_DECLARATION_INVALID;

   bool hasAnimationDef = false;
   bool hasAnimationName = false;
   bool hasAnimationKey = false;
   bool hasAnimationType = false;
   std::string animationDef;
   std::string animationName;
   std::string animationKey;
   std::uint32_t animationType = 0;
   if (!FindOptionalJsonStringProperty(json, declarationBegin,
                                       declarationEnd, "animationDef",
                                       hasAnimationDef, animationDef) ||
       !FindOptionalJsonStringProperty(json, declarationBegin,
                                       declarationEnd, "animationName",
                                       hasAnimationName, animationName) ||
       !FindOptionalJsonStringProperty(json, declarationBegin,
                                       declarationEnd, "animationKey",
                                       hasAnimationKey, animationKey) ||
       !FindOptionalJsonUnsignedProperty(json, declarationBegin,
                                         declarationEnd, "animationType",
                                         hasAnimationType, animationType))
      return EXTERNAL_DECLARATION_INVALID;

   const bool hasCustomAnimation = hasAnimationDef || hasAnimationName ||
      hasAnimationType;
   if (hasCustomAnimation)
   {
      if (!hasAnimationDef || !hasAnimationName || !hasAnimationType ||
          animationType > 0xFFFFu ||
          !IsTechnicalIdentity(animationDef) ||
          !IsTechnicalIdentity(animationName))
         return EXTERNAL_DECLARATION_INVALID;
      const std::string expectedKey = provider + "." + spellKey + "." +
         animationName;
      if (expectedKey.size() > 191 ||
          (hasAnimationKey && animationKey != expectedKey))
         return EXTERNAL_DECLARATION_INVALID;
   }
   else if (hasAnimationKey)
   {
      const std::string providerPrefix = provider + ".";
      if (!IsTechnicalIdentity(animationKey, 191) ||
          animationKey.compare(0, providerPrefix.size(), providerPrefix) != 0)
         return EXTERNAL_DECLARATION_INVALID;
   }

   bool hasTargetMode = false;
   std::string targetMode;
   if (!FindOptionalJsonStringProperty(json, declarationBegin,
                                       declarationEnd, "combatTargetMode",
                                       hasTargetMode, targetMode))
      return EXTERNAL_DECLARATION_INVALID;
   if (kind == "adventure")
   {
      if (hasTargetMode)
         return EXTERNAL_DECLARATION_INVALID;
   }
   else
   {
      const char* inferredMode = "global";
      if (spellFlags & SPELL_FLAG_SINGLE_TARGET)
         inferredMode = "targeted";
      else if (spellFlags & (SPELL_FLAG_TARGET_ANYWHERE |
                             SPELL_FLAG_AI_AREA_EFFECT))
         inferredMode = "area";
      else if (spellFlags & SPELL_FLAG_AI_CREATURES)
         inferredMode = "summon";
      if (hasTargetMode && targetMode != inferredMode)
         return EXTERNAL_DECLARATION_INVALID;

      const std::string selectedMode = hasTargetMode
         ? targetMode : std::string(inferredMode);
      const bool singleTarget =
         (spellFlags & SPELL_FLAG_SINGLE_TARGET) != 0;
      const bool areaTarget =
         (spellFlags & (SPELL_FLAG_TARGET_ANYWHERE |
                        SPELL_FLAG_AI_AREA_EFFECT)) != 0;
      const bool summonsCreatures =
         (spellFlags & SPELL_FLAG_AI_CREATURES) != 0;
      if (selectedMode == "targeted")
      {
         if (!singleTarget || areaTarget || summonsCreatures)
            return EXTERNAL_DECLARATION_INVALID;
      }
      else if (selectedMode == "area")
      {
         if (singleTarget || !areaTarget || summonsCreatures)
            return EXTERNAL_DECLARATION_INVALID;
      }
      else if (selectedMode == "summon")
      {
         if (singleTarget || areaTarget || !summonsCreatures)
            return EXTERNAL_DECLARATION_INVALID;
      }
      else if (singleTarget || areaTarget || summonsCreatures)
         return EXTERNAL_DECLARATION_INVALID;
   }

   return editorVisible == 1 ? EXTERNAL_DECLARATION_VISIBLE :
      EXTERNAL_DECLARATION_HIDDEN;
}

void DiscoverExternalSpellsInFile(
   const char* basePath, const char* overlayPath,
   std::vector<ExternalSpellCandidate> (&candidates)
      [NewSpellsMap::EXTERNAL_SPELL_SLOT_COUNT])
{
   std::string baseJson;
   std::size_t baseRootBegin = 0;
   std::size_t baseRootEnd = 0;
   if (!LoadJsonDocument(basePath, baseJson, baseRootBegin, baseRootEnd))
      return;

   std::size_t newSpellsBegin = 0;
   std::size_t newSpellsEnd = 0;
   std::size_t declarationsBegin = 0;
   std::size_t declarationsEnd = 0;
   if (!FindJsonObject(baseJson, baseRootBegin, baseRootEnd,
                       "NewSpells", newSpellsBegin, newSpellsEnd) ||
       !FindJsonObject(baseJson, newSpellsBegin, newSpellsEnd,
                       "ExternalSpells", declarationsBegin,
                       declarationsEnd))
      return;

   std::string overlayJson;
   std::size_t overlayRootBegin = 0;
   std::size_t overlayRootEnd = 0;
   const bool hasOverlay = overlayPath && *overlayPath &&
      LoadJsonDocument(overlayPath, overlayJson,
                       overlayRootBegin, overlayRootEnd);

   for (int spellId = NewSpellsMap::EXTERNAL_SPELL_ID_FIRST;
        spellId <= NewSpellsMap::EXTERNAL_SPELL_ID_LAST; ++spellId)
   {
      char spellKey[16] = {};
      if (sprintf_s(spellKey, sizeof(spellKey), "%d", spellId) < 0)
         continue;

      std::size_t declarationBegin = 0;
      std::size_t declarationEnd = 0;
      if (FindJsonMember(baseJson, declarationsBegin, declarationsEnd,
                         spellKey, declarationBegin,
                         declarationEnd) != JSON_MEMBER_FOUND ||
          declarationBegin >= declarationEnd ||
          baseJson[declarationBegin] != '{')
         continue;

      std::string name;
      std::uint32_t spellFlags = 0;
      if (!ValidateStandardSpellRecord(baseJson, baseRootBegin,
                                       baseRootEnd, spellId, name,
                                       spellFlags))
         continue;

      const ExternalDeclarationStatus declarationStatus =
         ValidateExternalDeclaration(baseJson, declarationBegin,
                                     declarationEnd, spellFlags);
      if (declarationStatus == EXTERNAL_DECLARATION_INVALID)
         continue;

      if (hasOverlay)
      {
         std::string translated;
         if (FindSpellName(overlayJson, overlayRootBegin, overlayRootEnd,
                           spellId, translated))
            name = translated;
      }

      ExternalSpellCandidate candidate;
      candidate.spellId = spellId;
      candidate.utf8Name = name;
      candidate.editorVisible =
         declarationStatus == EXTERNAL_DECLARATION_VISIBLE;
      candidates[spellId - NewSpellsMap::EXTERNAL_SPELL_ID_FIRST]
         .push_back(candidate);
   }
}

void DiscoverExternalSpells(
   const char* gameRoot, const std::string& locale,
   std::vector<ExternalSpellCandidate> (&candidates)
      [NewSpellsMap::EXTERNAL_SPELL_SLOT_COUNT])
{
   std::vector<std::string> mods;
   if (!ReadActiveModNames(gameRoot, mods))
      return;

   for (std::size_t modIndex = 0; modIndex < mods.size(); ++modIndex)
   {
      char relative[MAX_PATH];
      char langPath[MAX_PATH];
      char pattern[MAX_PATH];
      if (sprintf_s(relative, sizeof(relative), "Mods\\%s\\Lang",
                    mods[modIndex].c_str()) < 0 ||
          !MakePath(langPath, sizeof(langPath), gameRoot, relative) ||
          sprintf_s(pattern, sizeof(pattern), "%s\\*NewSpells.json",
                    langPath) < 0)
         continue;

      WIN32_FIND_DATAA file = {};
      HANDLE search = FindFirstFileA(pattern, &file);
      if (search == INVALID_HANDLE_VALUE)
         continue;

      do
      {
         if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

         char basePath[MAX_PATH];
         if (sprintf_s(basePath, sizeof(basePath), "%s\\%s", langPath,
                       file.cFileName) < 0)
            continue;

         char overlayPath[MAX_PATH] = {};
         if (!locale.empty() &&
             sprintf_s(relative, sizeof(relative),
                       "Mods\\%s\\Lang\\%s\\%s",
                       mods[modIndex].c_str(), locale.c_str(),
                       file.cFileName) >= 0)
            MakePath(overlayPath, sizeof(overlayPath), gameRoot, relative);

         DiscoverExternalSpellsInFile(basePath, overlayPath, candidates);
      }
      while (FindNextFileA(search, &file));

      FindClose(search);
   }
}

bool HasUniqueVisibleCandidate(
   const std::vector<ExternalSpellCandidate>& candidates)
{
   return candidates.size() == 1 && candidates[0].editorVisible;
}

bool LoadSpellCatalog()
{
   if (g_spellCatalogLoaded)
      return true;

   char modRoot[MAX_PATH];
   char gameRoot[MAX_PATH];
   char path[MAX_PATH];
   if (!GetModRoot(modRoot, sizeof(modRoot)) ||
       !GetGameRoot(gameRoot, sizeof(gameRoot)) ||
       !MakePath(path, sizeof(path), modRoot, "Lang\\NewSpells.json"))
      return false;

   std::string baseJson;
   std::size_t baseRootBegin = 0;
   std::size_t baseRootEnd = 0;
   if (!LoadJsonDocument(path, baseJson, baseRootBegin, baseRootEnd))
      return false;

   std::vector<ExternalSpellCandidate> utf8Catalog;
   utf8Catalog.reserve(NewSpellsMap::BUILTIN_SPELL_COUNT +
                       NewSpellsMap::EXTERNAL_SPELL_SLOT_COUNT);
   for (std::size_t i = 0; i < NewSpellsMap::BUILTIN_SPELL_COUNT; ++i)
   {
      ExternalSpellCandidate entry;
      entry.spellId = NewSpellsMap::BUILTIN_SPELL_IDS[i];
      entry.editorVisible = true;
      if (!FindSpellName(baseJson, baseRootBegin, baseRootEnd,
                         entry.spellId, entry.utf8Name))
         return false;
      utf8Catalog.push_back(entry);
   }

   const std::string locale = LanguageFolder(gameRoot);
   if (!locale.empty())
   {
      char relative[MAX_PATH];
      if (sprintf_s(relative, sizeof(relative),
                    "Lang\\%s\\NewSpells.json", locale.c_str()) >= 0 &&
          MakePath(path, sizeof(path), modRoot, relative))
      {
         std::string overlayJson;
         std::size_t overlayRootBegin = 0;
         std::size_t overlayRootEnd = 0;
         if (LoadJsonDocument(path, overlayJson,
                              overlayRootBegin, overlayRootEnd))
         {
            for (std::size_t i = 0; i < utf8Catalog.size(); ++i)
            {
               std::string translated;
               if (FindSpellName(overlayJson, overlayRootBegin,
                                 overlayRootEnd, utf8Catalog[i].spellId,
                                 translated))
                  utf8Catalog[i].utf8Name = translated;
            }
         }
      }
   }

   std::vector<ExternalSpellCandidate>
      externalCandidates[NewSpellsMap::EXTERNAL_SPELL_SLOT_COUNT];
   DiscoverExternalSpells(gameRoot, locale, externalCandidates);
   for (std::size_t i = 0;
        i < NewSpellsMap::EXTERNAL_SPELL_SLOT_COUNT; ++i)
   {
      // A duplicated external ID has no deterministic owner.  Omit every
      // claimant instead of making the result depend on Mods/list.txt order.
      if (HasUniqueVisibleCandidate(externalCandidates[i]))
         utf8Catalog.push_back(externalCandidates[i][0]);
   }

   char iniPath[MAX_PATH];
   if (!MakePath(iniPath, sizeof(iniPath), gameRoot, "heroes3.ini"))
      return false;
   UINT codePage = static_cast<UINT>(GetPrivateProfileIntA(
      "Era", "CodePage", GetACP(), iniPath));
   if (!IsValidCodePage(codePage))
      codePage = GetACP();

   std::vector<SpellCatalogEntry> converted;
   converted.reserve(utf8Catalog.size());
   for (std::size_t i = 0; i < utf8Catalog.size(); ++i)
   {
      SpellCatalogEntry entry;
      entry.spellId = utf8Catalog[i].spellId;
      if (!Utf8ToCodePage(utf8Catalog[i].utf8Name, codePage, entry.name))
         return false;
      converted.push_back(entry);
   }

   g_spellCatalog.swap(converted);
   g_spellCatalogLoaded = true;
   return true;
}

std::size_t FindCatalogIndex(const int spellId)
{
   for (std::size_t i = 0; i < g_spellCatalog.size(); ++i)
      if (g_spellCatalog[i].spellId == spellId)
         return i;
   return static_cast<std::size_t>(-1);
}

bool IsCatalogSpell(const int spellId)
{
   return FindCatalogIndex(spellId) != static_cast<std::size_t>(-1);
}

void* ChecklistObject(void* page)
{
   return static_cast<unsigned char*>(page) + CHECKLIST_OBJECT_OFFSET;
}

HWND ChecklistWindow(void* page)
{
   return *reinterpret_cast<HWND*>(
      static_cast<unsigned char*>(page) + CHECKLIST_HWND_OFFSET);
}

bool ValidateChecklist(void* page)
{
   if (!page)
      return false;

   const HWND list = ChecklistWindow(page);
   if (!IsWindow(list) || GetDlgCtrlID(list) != SPELL_CHECKLIST_ID)
      return false;

   char className[32] = {};
   if (!GetClassNameA(list, className, sizeof(className)) ||
       _stricmp(className, "ListBox") != 0)
      return false;

   const LONG style = GetWindowLongA(list, GWL_STYLE);
   return (style & LBS_MULTICOLUMN) != 0 &&
      (style & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) != 0;
}

void SetCheck(void* page, const int index, const int value)
{
   typedef void (__thiscall *SetCheckFunction)(void*, int, int);
   reinterpret_cast<SetCheckFunction>(Address(0x515D35))(
      ChecklistObject(page), index, value);
}

int GetCheck(void* page, const int index)
{
   typedef int (__thiscall *GetCheckFunction)(void*, int);
   return reinterpret_cast<GetCheckFunction>(Address(0x515DB0))(
      ChecklistObject(page), index);
}

void MarkDocumentModified(void* document)
{
   if (!document)
      return;

   typedef void (__thiscall *SetModifiedFunction)(void*, int);
   reinterpret_cast<SetModifiedFunction>(Address(0x45ECE4))(document, 1);
}

bool CollectNewSpellChecks(void* page, NewSpellsMap::State& state)
{
   if (!ValidateChecklist(page))
      return false;

   // Only displayed spells are editable.  Start from the loaded trailer so
   // disabled bits owned by absent or future providers remain byte-for-byte
   // intact when this page is applied.
   state = g_disabledSpells;
   std::vector<unsigned char> found(g_spellCatalog.size(), 0);
   const HWND list = ChecklistWindow(page);
   const LRESULT count = SendMessageA(list, LB_GETCOUNT, 0, 0);
   if (count == LB_ERR)
      return false;

   for (int index = 0; index < count; ++index)
   {
      const int spellId = static_cast<int>(
         SendMessageA(list, LB_GETITEMDATA, index, 0));
      const std::size_t catalogIndex = FindCatalogIndex(spellId);
      if (catalogIndex != static_cast<std::size_t>(-1))
      {
         if (found[catalogIndex])
            return false;
         found[catalogIndex] = 1;
         NewSpellsMap::SetDisabled(state, spellId,
            GetCheck(page, index) == 0);
      }
   }

   for (std::size_t i = 0; i < found.size(); ++i)
      if (!found[i])
         return false;

   return true;
}

void RemoveCatalogRows(const HWND list)
{
   const int count = static_cast<int>(SendMessageA(list, LB_GETCOUNT, 0, 0));
   for (int index = count - 1; index >= 0; --index)
   {
      const int spellId = static_cast<int>(
         SendMessageA(list, LB_GETITEMDATA, index, 0));
      if (IsCatalogSpell(spellId))
         SendMessageA(list, LB_DELETESTRING, index, 0);
   }
}

bool PopulateNewSpellChecks(void* page)
{
   if (!ValidateChecklist(page) || !LoadSpellCatalog() ||
       g_spellCatalog.empty())
      return false;

   const HWND list = ChecklistWindow(page);
   const LRESULT originalCountResult = SendMessageA(list, LB_GETCOUNT, 0, 0);
   if (originalCountResult == LB_ERR)
      return false;
   const int originalCount = static_cast<int>(originalCountResult);

   std::vector<int> indices(g_spellCatalog.size(), -1);
   int existing = 0;
   for (int index = 0; index < originalCount; ++index)
   {
      const int spellId = static_cast<int>(
         SendMessageA(list, LB_GETITEMDATA, index, 0));
      const std::size_t catalogIndex = FindCatalogIndex(spellId);
      if (catalogIndex != static_cast<std::size_t>(-1))
      {
         if (indices[catalogIndex] != -1)
            return false;
         indices[catalogIndex] = index;
         ++existing;
      }
   }

   if (existing != 0 &&
       existing != static_cast<int>(g_spellCatalog.size()))
      return false;

   if (existing == 0)
   {
      for (std::size_t i = 0; i < g_spellCatalog.size(); ++i)
      {
         const LRESULT index = SendMessageA(list, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(g_spellCatalog[i].name.c_str()));
         if (index == LB_ERR || index == LB_ERRSPACE)
         {
            RemoveCatalogRows(list);
            return false;
         }

         if (SendMessageA(list, LB_SETITEMDATA, index,
             g_spellCatalog[i].spellId) == LB_ERR)
         {
            SendMessageA(list, LB_DELETESTRING, index, 0);
            RemoveCatalogRows(list);
            return false;
         }
      }

      // LBS_SORT moves earlier insertions as later names are added, so locate
      // every item again before applying check states.
      std::fill(indices.begin(), indices.end(), -1);
      const int finalCount = static_cast<int>(
         SendMessageA(list, LB_GETCOUNT, 0, 0));
      for (int index = 0; index < finalCount; ++index)
      {
         const int spellId = static_cast<int>(
            SendMessageA(list, LB_GETITEMDATA, index, 0));
         const std::size_t catalogIndex = FindCatalogIndex(spellId);
         if (catalogIndex != static_cast<std::size_t>(-1))
         {
            if (indices[catalogIndex] != -1)
            {
               RemoveCatalogRows(list);
               return false;
            }
            indices[catalogIndex] = index;
         }
      }

      for (std::size_t i = 0; i < g_spellCatalog.size(); ++i)
         if (indices[i] == -1)
         {
            RemoveCatalogRows(list);
            return false;
         }
   }

   for (std::size_t i = 0; i < g_spellCatalog.size(); ++i)
      SetCheck(page, indices[i],
         NewSpellsMap::IsDisabled(g_disabledSpells,
            g_spellCatalog[i].spellId) ? 0 : 1);

   return true;
}

int __stdcall OnSpellsPageInit(HiHook* hook, void* page)
{
   const int result = CALL_1(int, __thiscall, hook->GetDefaultFunc(), page);
   if (g_active && result)
      PopulateNewSpellChecks(page);
   return result;
}

int __stdcall OnSpellsPageApply(HiHook* hook, void* page)
{
   // The native apply routine consumes/resynchronizes the checklist state.
   // Snapshot our appended rows first, but commit only if native validation
   // succeeds so OK remains transactional and Cancel remains untouched.
   NewSpellsMap::State state;
   const bool collected = g_active && CollectNewSpellChecks(page, state);
   const int result = CALL_1(int, __thiscall, hook->GetDefaultFunc(), page);
   if (!g_active || !result || !collected)
      return result;

   if (!NewSpellsMap::Equals(state, g_disabledSpells))
   {
      g_disabledSpells = state;
      MarkDocumentModified(g_currentDocument);
   }
   return result;
}

int __stdcall OnSpellsCheckChange(HiHook* hook, void* page)
{
   if (g_active && ValidateChecklist(page))
   {
      const HWND list = ChecklistWindow(page);
      const LRESULT index = SendMessageA(list, LB_GETCURSEL, 0, 0);
      if (index != LB_ERR)
      {
         const int spellId = static_cast<int>(
            SendMessageA(list, LB_GETITEMDATA, index, 0));
         if (IsCatalogSpell(spellId))
            return 0;
      }
   }

   return CALL_1(int, __thiscall, hook->GetDefaultFunc(), page);
}

int __stdcall OnNewDocument(HiHook* hook, void* document)
{
   const int result = CALL_1(int, __thiscall,
      hook->GetDefaultFunc(), document);
   if (g_active && result)
   {
      g_currentDocument = document;
      NewSpellsMap::Clear(g_disabledSpells);
   }
   return result;
}

int __stdcall OnOpenDocument(HiHook* hook, void* document,
                             const char* path)
{
   const int result = CALL_2(int, __thiscall,
      hook->GetDefaultFunc(), document, path);
   if (g_active && result)
   {
      g_currentDocument = document;
      NewSpellsMap::Clear(g_disabledSpells);
      NewSpellsMap::ReadTrailerFileA(path, g_disabledSpells);
   }
   return result;
}

int __stdcall OnSaveDocument(HiHook* hook, void* document,
                             const char* path)
{
   const int result = CALL_2(int, __thiscall,
      hook->GetDefaultFunc(), document, path);
   if (!g_active || !result)
      return result;

   g_currentDocument = document;
   if (!NewSpellsMap::RewriteTrailerFileA(path, g_disabledSpells))
   {
      MarkDocumentModified(document);
      return 0;
   }
   return result;
}

bool InstallHooks()
{
   if (!ValidateEditorImage())
      return false;

   HiHook* hooks[] =
   {
      g_instance->WriteHiHook(Address(0x47666E), SPLICE_, EXTENDED_,
         THISCALL_, OnSpellsPageInit),
      g_instance->WriteHiHook(Address(0x47680C), SPLICE_, EXTENDED_,
         THISCALL_, OnSpellsPageApply),
      g_instance->WriteHiHook(Address(0x476860), SPLICE_, EXTENDED_,
         THISCALL_, OnSpellsCheckChange),
      g_instance->WriteHiHook(Address(0x45EF8D), SPLICE_, EXTENDED_,
         THISCALL_, OnNewDocument),
      g_instance->WriteHiHook(Address(0x45F3BB), SPLICE_, EXTENDED_,
         THISCALL_, OnOpenDocument),
      g_instance->WriteHiHook(Address(0x45F3DE), SPLICE_, EXTENDED_,
         THISCALL_, OnSaveDocument)
   };

   for (std::size_t i = 0; i < sizeof(hooks) / sizeof(hooks[0]); ++i)
      if (!hooks[i])
         return false;

   return true;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
   if (reason == DLL_PROCESS_ATTACH)
   {
      g_module = module;
      DisableThreadLibraryCalls(module);
      NewSpellsMap::Clear(g_disabledSpells);

      g_patcher = GetPatcher();
      if (g_patcher)
      {
         char owner[] = "NewSpells.MapEditor";
         g_instance = g_patcher->CreateInstance(owner);
         if (g_instance)
            g_active = InstallHooks();
      }
   }

   return TRUE;
}
