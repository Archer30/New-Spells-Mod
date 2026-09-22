# Spell ids up to 199

New Spells addresses spell ids 0..199, the size of the WoG spell table. Earlier versions
stopped at 126 because several places in the executable and in the plugin encode a smaller
limit. This note lists each limit and how the plugin lifts it, so a maintainer can find the
mechanism behind a symptom.

## The ladder of limits

| Limit | Where | Mechanism now |
|---|---|---|
| 70 | the hero's `in_spellbook[70]` and `available_spells[70]` arrays | Kept at their original offsets (0x3EA, 0x430). Ids 70 and up live in a side table keyed by hero id, the 28 executable sites that index the arrays are hooked (`NsHeroSpells.h`), the save game carries the table and the campaign crossover copies it. |
| 96 | the 12-byte `std::bitset<70>` objects of the map loader and the artifact routines | The loops that fill them keep the executable's bound, the artifact updater re-adds ids 70 and up from the plugin (`addArtifactSpells`). |
| 127 | 29 `cmp reg, imm8` compares whose immediate is the spell count | Each compare and its branch is replaced by a hook that compares against the live count (`NsSpellBounds.h`). A site that another plugin changed is skipped and logged, that site then keeps the executable's bound of 70. |
| 140 | the 140 disabled-spell bytes at `Game+4` | Ids 140 and up live in `nsDisabledEx`, the pyramid, scholar and mage guild readers and `UN:J0` are hooked (`NsDisabledSpells.h`). |
| 162 | `army::spellInfluence[162]` (it already overlays the mastery array up to the influence queue) | Ids 162 and up live in `nsDurationsEx` keyed by side and stack index, AI copies in a small pool keyed by address (`NsDurations.h`). Every plugin access goes through `nsDuration`, the four executable readers are hooked, and the executable loops stop at 162 and hooks after them repeat the work for the side table (`NsDurationHooks.h`). |
| 200 | the WoG spell table, `SS`, `SN:H` | The ceiling. `SPELLS_MAX` is 256 for array sizing, `NS_MAX_SPELL_ID` is 199. |

## Runtime count

`activeSpellCount` is the physical spell count: `NewSpells.Config.MaxSpellId` + 1, raised to
the highest valid external or data declaration + 1. ERA loads the Lang json after the plugins,
so the count and everything that depends on it (the executable immediates of
`writeSpellCountPatches`, the data spells, the options, the provider registration of the data
spells) are set in the `OnAfterWoG` handler `nsAfterWogStartup`, before the executable's game
initialization runs. ERM reads the count from `i^NewSpells.SpellCount^`.

## Known cosmetic and external limits

- The stack drawing loop tints a stack for active spells with ids below 162 only.
- The HD mod battle replay stores per-stack durations with its own layout, ids 162 and up are
  not part of a replay.
- Scroll icons: `SpellScr.def` keeps its "All Spells" frame at index 127, a pack that uses id
  127 should override that frame.
- Emerald's level spell-set artifacts stop at spell 69 inside Emerald's own loops.

## Validation

`tests/ProbeSpellCeiling.ps1` runs a probe build of the core (`-p:CeilingNativeProbe=true`,
`tests/SpellCeilingNativeProbe.inl`) in a disposable game copy under `work/`. Inside the live
process it checks the count, every hook site, the side tables, the padded icon sheets and the
data spells, and writes `Ceiling native probe: Result | PASS` to ERA's log. ERA writes
`Debug\Era\log.txt` only when the game closes, so the probe closes the game before reading it.
