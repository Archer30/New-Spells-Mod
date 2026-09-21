#include "../NewSpells/AnimationCatalog.h"
#include <climits>
struct Record { const char* defName; const char* name; int type; };
Record source[3000] = {}, output[4096] = {};
char defs[2000][20], names[2000][20];
int main()
{
   int checks = 0;
#define CHECK(x) do { ++checks; if (!(x)) { std::printf("FAILED line %d: %s\n", __LINE__, #x); return 1; } } while (0)
   for (int i = 0; i < 1000; ++i) source[i] = {"existing.def", "Existing", i};
   for (int i = 1000; i < 3000; ++i)
   {
      sprintf_s(defs[i-1000], "anim%d.def", i);
      sprintf_s(names[i-1000], "anim%d", i);
      source[i] = {defs[i-1000], names[i-1000], i >= 2000 ? 257 : 1};
   }
   CHECK(AnimationCatalog::ReservedRowsMatch(source));
   source[2999].type = 1;
   CHECK(!AnimationCatalog::ReservedRowsMatch(source));
   source[2999].type = 257;
   source[1000].defName = 0;
   CHECK(!AnimationCatalog::ReservedRowsMatch(source));
   source[1000].defName = defs[0];
   names[1999][0] = 'X';
   CHECK(!AnimationCatalog::ReservedRowsMatch(source));
   names[1999][0] = 'a';
   CHECK(AnimationCatalog::Preserve(output, 4096, source, 3000, 9));
   CHECK(std::memcmp(source, output, sizeof(source)) == 0);
   CHECK(output[3000].defName == 0);
   CHECK(AnimationCatalog::Preserve(output, 128, source, 87, 9));
   CHECK(!AnimationCatalog::Preserve(output, 128, source, 3000, 9));
   CHECK(!AnimationCatalog::Preserve(output, 3008, source, 3000, 9));
   CHECK(!AnimationCatalog::Preserve(output, INT_MAX, source, 3000, INT_MAX));
   CHECK(!AnimationCatalog::Preserve(output, 4096, source, -1, 9));
   CHECK(!AnimationCatalog::Preserve(output, 4096, source, 3000, -1));
   CHECK(!AnimationCatalog::Preserve(static_cast<Record*>(0), 4096, source, 3000, 9));
   std::printf("Passed %d animation catalog checks (native/WoG, extended catalog, preservation, malformed records, capacity and overflow).\n", checks);
}
