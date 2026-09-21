# Reinforcements 2.8 import and Town Portal hook record (superseded)

> Historical record only. New Spells 2.9 removed this ERM/Town Portal bridge.
> The active implementation and current signatures are documented in
> `REINFORCEMENTS_NATIVE_PORT.md`; New Spells 2.12 packages that implementation
> in the separate New Spells Expansion provider.

## Scope

Reinforcements Spell 2.3 originally replaces Disguise (spell ID 4) and uses
ERM Hooker callbacks inside Disguise and Town Portal. New Spells 2.8 instead
owns Reinforcements as appended spell ID 96. The existing New Spells adventure
spell dispatcher calls the imported ERM dialog function through ERA's
`AllocErmFunc`/`FireErmEvent` API, so Disguise remains untouched.

## Town-selection seam

- Module owner / build evidence: current `D:\Games\Heroes 3 ERA\h3era.exe`,
  PE32 (`IMAGE_FILE_MACHINE_I386`), preferred image base `0x00400000`, seven
  sections, PE timestamp `1996-02-26 04:38:09Z` (evidence only, not a gate).
- Historical source: the donor script hooked decimal address `4315531`
  (`0x0041D98B`). The paired `homm3` IDB identifies `0x0041D4E0..0x0041DAA8`
  as `Cast_TownPortal(H3AdventureManager*, signed int)`.
- Current address domains: preferred/runtime VA `0x0041D98B` in the fixed-base
  executable, RVA `0x0001D98B`, `.text` file offset `0x0001D98B`.
- Register contract: after Town Portal returns a valid selection, EDI is the
  selected `H3Town*`. Reinforcements needs this pointer but must skip the stock
  teleport, movement, and mana path.
- Original file bytes at the hook seam:
  `66 0F B6 4F 05 8B 45 EC 8B 5D D0 66`. The first instruction is exactly
  five bytes, so the LoHook starts and ends on instruction boundaries.
- Extension action: store EDI in ERM associated variable
  `rei_town_struct`, then return to `0x0041DA95` only while
  `rei_in_town_cast` is set. `0x0041DA95` begins the verified function
  epilogue (`8B 4D F4 5F 5E 5B ...`). Ordinary Town Portal casts execute the
  original instruction and control flow.
- Runtime gating: New Spells compares the complete 12-byte seam before
  installing the LoHook and records `IsApplied()`. Reinforcements is disabled
  if that contract is unavailable. The standalone donor mod is also declared
  incompatible, preventing its raw hook from competing at the same site.
- Startup verification (2026-08-29): a hidden ERA launch loaded the rebuilt
  `NewSpells.dll`, exposed 97 live spell records (last ID 96), left
  Reinforcements enabled, and showed an applied jump at `0x0041D98B`. ERA's
  startup log contained no script or plugin errors.
- Remaining interactive verification: exercise closest-town and selectable-town
  paths, cancel, successful transfer, garrisoned heroes, daily reset,
  save/load, multiplayer ownership, and inspect the live Patcher_x86 chain.

No executable filename, version string, checksum, or whole-file hash is used
as a compatibility gate.
