#pragma once

#include "../NewSpells/NewSpellsProviderApi.h"

class Patcher;

namespace Blizzard
{
   const int SpellId = 97;
   const unsigned Capabilities = 0x7FE;
   bool Initialize(Patcher* patcher);
   void Shutdown();
   NewSpellsProviderSpellV1 Descriptor(const char* providerKey);
}
