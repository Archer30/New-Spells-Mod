#pragma once

#include <cstdint>

// Original ERM BM:G addresses use x86 wrapping arithmetic. Only the old
// mastery cells move; every other legacy address keeps its original meaning.
namespace NewSpellsErm
{
   const std::uint32_t StackSize = 0x548;
   const std::uint32_t StackCount = 42;
   const std::uint32_t DurationOffset = 0x198;
   const std::uint32_t MasteryOffset = 0x2DC;
   const std::uint32_t OriginalSpellCount = 81;

   inline std::uint32_t LegacyAddress(const std::uint32_t stack,
      const std::int32_t index, const std::uint32_t offset)
   {
      return stack + offset + static_cast<std::uint32_t>(index) * 4u;
   }

   inline bool OriginalMasterySlot(const std::uint32_t address,
      const std::uint32_t stacks, std::uint32_t& slot, std::uint32_t& spell)
   {
      const std::uint32_t relative = address - stacks;
      if (relative >= StackCount * StackSize)
         return false;
      const std::uint32_t field = relative % StackSize;
      if (field < MasteryOffset ||
          field >= MasteryOffset + OriginalSpellCount * 4u ||
          (field - MasteryOffset) % 4u != 0)
         return false;
      slot = relative / StackSize;
      spell = (field - MasteryOffset) / 4u;
      return true;
   }
}
