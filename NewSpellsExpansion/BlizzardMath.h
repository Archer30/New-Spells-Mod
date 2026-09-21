#pragma once

#include <stdint.h>
#include <limits.h>

// Pure, bounded operations shared by the provider and its regression tests.
namespace BlizzardMath
{
   inline int Clamp(int64_t n)
   {
      return n > INT_MAX ? INT_MAX : n < INT_MIN ? INT_MIN : static_cast<int>(n);
   }
   inline int Positive(int64_t n) { return n < 0 ? 0 : Clamp(n); }
   inline bool Hex(int hex) { return hex >= 0 && hex < 187 && hex % 17 != 0 && hex % 17 != 16; }
   inline int Penalty(int speed, int requested)
   {
      return speed <= 1 || requested <= 0 ? 0 : (speed - 1 < requested ? speed - 1 : requested);
   }
   inline int BaseDamage(int power, int factor, int bonus)
   {
      return Positive(static_cast<int64_t>(power) * factor + bonus);
   }
   inline int AreaValue(int64_t enemy, int64_t friendly, int enemyArmy, int friendlyArmy)
   {
      if (enemy <= 0 || friendly >= enemy || enemyArmy <= 0 || friendlyArmy <= 0)
         return 0;
      if (friendly < 0) friendly = 0;
      if (friendly >= friendlyArmy ||
          static_cast<double>(friendly) / friendlyArmy >= static_cast<double>(enemy) / enemyArmy)
         return 0;
      return Positive(enemy - friendly);
   }
}
