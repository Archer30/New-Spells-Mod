# Fortitude provider

The built-in Toughness spell written as an external provider DLL for spell id 151. It is the
C++ counterpart of `examples/New Spells Sample Pack`, which defines the same spell as a data
spell (id 150) without any code. Compare the two to decide which tier a new spell needs.

What the DLL does:

- `ValidateCombatTarget` refuses war machines.
- `CastCombat` applies the status through the engine's own `ApplySpell`, plays the spell's
  animation and writes the combat log line, then returns `COMMITTED` so New Spells finishes
  the cast in the engine's epilogue (mana and the once-per-round flag stay with the engine).
- `OnStatusApply` and `OnStatusRemove` set the hit points of one creature to 130..150 percent
  of the base value and back. The data-spell version composes with Age, Toughness and Hour of
  Power because it lives inside the core's health chain, a provider owns its own arithmetic.
- `EvaluateCombatAi` gives the AI a rough value for friendly stacks without the spell.

Resources: the JSON record next to this file goes to `Lang/` of the provider mod, the icon
frames and the animation def come from the sample pack (`Data/Defs/*/0_15x.png` renamed for
id 151 and `Data/NewSpells/Files/SmpTough.def`). `capabilities` 1134 is the decimal mask of
combat target, combat cast, status apply, status remove, cure/dispel and combat AI, and must
match the descriptor.

Build it with the SDK solution, the DLL goes to `EraPlugins/` of the provider mod, which must
require and load after New Spells.
