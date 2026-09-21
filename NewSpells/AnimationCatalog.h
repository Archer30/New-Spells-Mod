#pragma once
#include <cstring>
#include <cstdio>

namespace AnimationCatalog
{
   template<class Record> bool ReservedRowsMatch(const Record* table)
   {
      for (int i = 1000; i < 3000; ++i)
      {
         char def[20], name[20];
         sprintf_s(def, "anim%d.def", i);
         sprintf_s(name, "anim%d", i);
         if (!table[i].defName || !table[i].name ||
             std::strcmp(table[i].defName, def) || std::strcmp(table[i].name, name) ||
             table[i].type != (i >= 2000 ? 0x101 : 1)) return false;
      }
      return true;
   }
   template<class Record> bool Preserve(Record* destination, int capacity,
      const Record* source, int count, int appended)
   {
      if (!destination || !source || capacity <= 0 || count < 83 ||
          appended < 0 || count > capacity || appended > capacity - count) return false;
      std::memcpy(destination, source, static_cast<size_t>(count) * sizeof(Record));
      return true;
   }
}
