#include "../NewSpellsExpansion/BlizzardMath.h"
#include <stdio.h>

int main()
{
   using namespace BlizzardMath;
   int checks = 0;
#define CHECK(expr) do { ++checks; if (!(expr)) { printf("FAIL line %d: %s\n", __LINE__, #expr); return 1; } } while (0)
   const int bonuses[] = {30, 30, 60, 120};
   for (int mastery = 0; mastery < 4; ++mastery) {
      CHECK(BaseDamage(10, 20, bonuses[mastery]) == 200 + bonuses[mastery]);
      CHECK(BaseDamage(0, 20, bonuses[mastery]) == bonuses[mastery]);
   }
   CHECK(BaseDamage(INT_MAX, INT_MAX, INT_MAX) == INT_MAX);
   CHECK(BaseDamage(1, -20, 0) == 0);
   CHECK(Penalty(10, 2) == 2);
   CHECK(Penalty(10, 4) == 4);
   CHECK(Penalty(3, 4) == 2);
   CHECK(Penalty(1, 4) == 0);
   CHECK(Penalty(0, 4) == 0);
   CHECK(Penalty(INT_MIN, 4) == 0);
   CHECK(AreaValue(100, 0, 1000, 1000) == 100);
   CHECK(AreaValue(100, 40, 1000, 1000) == 60);
   CHECK(AreaValue(100, 100, 1000, 1000) == 0);
   CHECK(AreaValue(100, 110, 1000, 1000) == 0);
   CHECK(AreaValue(100, 40, 1000, 100) == 0); // proportionately worse friendly loss
   CHECK(AreaValue(100, 0, 0, 1000) == 0);
   CHECK(AreaValue(100, 0, 1000, 0) == 0);
   CHECK(AreaValue(0, -50, 1000, 1000) == 0);
   CHECK(AreaValue(INT_MAX * int64_t(42), 0, INT_MAX, INT_MAX) == INT_MAX);
   int legal = 0;
   for (int h = -1; h <= 187; ++h) if (Hex(h)) ++legal;
   CHECK(legal == 165);
   CHECK(!Hex(0) && !Hex(16) && !Hex(17) && !Hex(186));
   CHECK(Hex(1) && Hex(15) && Hex(18) && Hex(185));
   printf("Passed %d Blizzard arithmetic and target-boundary checks.\n", checks);
   return 0;
}
