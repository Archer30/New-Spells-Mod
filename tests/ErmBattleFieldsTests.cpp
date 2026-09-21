#include "../NewSpells/ErmBattleFields.h"
#include <cstdio>
#include <climits>

int main()
{
   using namespace NewSpellsErm;
   unsigned checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
   const std::uint32_t base = 0x12340000;
   std::uint32_t slot = 0, spell = 0;
   for (std::uint32_t s = 0; s < StackCount; ++s)
      for (std::uint32_t byte = 0; byte < StackSize; ++byte)
      {
         const bool expected = byte >= 0x2DC && byte < 0x420 && byte % 4 == 0;
         CHECK(OriginalMasterySlot(base + s * StackSize + byte, base, slot, spell) == expected);
         if (expected) { CHECK(slot == s); CHECK(spell == (byte - 0x2DC) / 4); }
      }
   CHECK(!OriginalMasterySlot(base - 4, base, slot, spell));
   CHECK(!OriginalMasterySlot(base + StackCount * StackSize, base, slot, spell));
   CHECK(LegacyAddress(base, 212, DurationOffset) == base + 1256);
   CHECK(LegacyAddress(base, 213, DurationOffset) == base + 1260);
   CHECK(LegacyAddress(base, -81, DurationOffset) == base + 84);
   CHECK(LegacyAddress(base, -95, DurationOffset) == base + 28);
   CHECK(LegacyAddress(base, -103, DurationOffset) == base - 4);
   CHECK(LegacyAddress(base, 236, DurationOffset) == base + StackSize);
   CHECK(LegacyAddress(base, INT_MIN, DurationOffset) == base + DurationOffset);
   CHECK(LegacyAddress(base, INT_MAX, DurationOffset) == base + DurationOffset - 4);
   CHECK(LegacyAddress(0xFFFFFFFCu, 1, DurationOffset) == DurationOffset);
   for (int index = 127; index <= 161; ++index)
   {
      CHECK(OriginalMasterySlot(LegacyAddress(base, index, DurationOffset), base, slot, spell));
      CHECK(slot == 0 && spell == static_cast<unsigned>(index - 81));
   }
   // Index 465 reaches the next stack's original mastery[46].
   CHECK(OriginalMasterySlot(LegacyAddress(base, 465, DurationOffset), base, slot, spell));
   CHECK(slot == 1 && spell == 46);
   std::printf("PASS: %u BM:G address checks\n", checks);
   return 0;
}
