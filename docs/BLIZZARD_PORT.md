# Blizzard external provider (97)

Expansion 1.1.0 adds Blizzard alongside Reinforcements (96), using provider ABI
v1. The paired core is New Spells 2.12.2. Author of the donor: daemon_n, with
ShimmY; port and AI corrections: Archer30.

## Behavior

| Property | None | Basic | Advanced | Expert |
|---|---:|---:|---:|---:|
| Mana | 25 | 20 | 20 | 20 |
| Damage | 20 × Power + 30 | 20 × Power + 30 | 20 × Power + 60 | 20 × Power + 120 |
| Speed penalty | 2 | 2 | 4 | 4 |

Level 5, Water, radius 2 including the center. Friendly stacks are affected.
The native area spell machinery retains damage modifiers, immunity, and one
resistance roll per stack. Only surviving affected stacks receive a new penalty.
An existing penalty survives death/resurrection and repeated casts. Recasting
can update the recorded mastery, but does not increase the first subtraction.
Effective speed stays at least 1, including after Haste/Prayer expires.

Native influence insertion/removal owns counts and the information-dialog
queue. A separate sidecar records the actual speed subtraction. Round events
refresh the permanent duration; Cure/Dispel callbacks veto removal. Explicit
ERM BM:C changes apply/remove status only, without damage or area execution.
Battle boundaries clear state; a reused slot with a fresh influence table drops
the old sidecar before casting. The native InitClean hook also clears slot state
for fresh stacks, including status-only ERM use after a slot is reused. Reinforcements initializes independently.

## Native integration and AI

The donor replaced Remove Obstacle (64) and attached slowing to the damage call
at `0x5A4DED`. New Spells bypasses that call, so the provider instead hooks the
actual `army::Damage` function (`0x443DB0`) under its cast scope. No donor ID-64
replacement patches are included. The death-only cancellation call at
`0x443F58` preserves an existing Blizzard status; ordinary native removal still
performs complete queue/count cleanup.

The resistance adapter is inserted adjacent to the core's HiHook at
`0x5A83A0`. It bypasses only the core's external-spell full-chance shortcut,
using the core hook's default function. Earlier Amethyst/ERA and later
Emerald/other hooks remain in their chain. Initialization checks applied hook
types, addresses, spans, executable defaults and untouched native instruction
tails. The adjacency contract is checked again before casting/evaluation. A
mismatch leaves Blizzard unregistered; it does not disable Reinforcements.
ERA-guarded original trampolines remain intact.

Preview, execution and AI share explicit native radius-2 geometry. Collection
scans all 42 slots once, including second body hexes and slot 20. AI scans all
165 legal target cells in ascending order. It combines native damage valuation
with Slow's initiative and movement-delay valuation, adapted to the flat speed
subtraction and expected surviving fraction. Existing Blizzard statuses and
lethal damage add no slowing value. Friendly slowing/damage are subtracted,
and native relative-collateral safeguards reject unfavorable casts. Strictly
higher positive scores replace the candidate, making lowest-hex ties stable.
Evaluation does not cast, spend mana, apply status or call the native RNG.
Quick Combat keeps its existing exclusion for external spells.

Two required shared fixes were found during startup validation:

* The combat-dispatch signature now checks the configured spell bound instead
  of the fixed core-only immediate, allowing combat providers above 95.
* More Anims replaces both animation count compares and owns 3,000 records.
  The core validates the paired hooks and reserved table ranges, preserves all
  records, and appends its entries. The allocated catalog capacity is 4,096;
  stock/WoG layouts still use their original count. This prevents interpreting
  a hook jump displacement as a count. VirtualQuery validation walks every
  memory region because Patcher can split page protections inside the table.

## Assets

The manifest derives spell frames 97 and SpellInt frame 98. The donor DEF is
extracted unchanged from `blizzard.original.pac`, stored as `NSEBLIZ.def`, and
registered as `HD.Plugin.H3.NewSpellsExpansion.Blizzard.Area` (type 1).
`C18SPW0.def` and Remove Obstacle's animation 34 remain unchanged. The standard
JSON record retains the required fallback animation index 34; activation
replaces that field with the owned animation's allocated index. English,
Russian and Chinese descriptions ship in the existing expansion JSON files.
Donor sound, town probabilities and AI table defaults are retained.

## Validation and reproduction

Build `Release|Win32` in VS 2022 with `v143` and `/MT`. Shared
`Directory.Build.targets` builds the host MapToDbgmap converter and emits a
matching `DebugMaps/<plugin>.dbgmap`; linker `.map` files remain build-only.
The converter source comes from `D:\Repos\h3era_plugins\tools\MapToDbgmap`.

Run `tests/ValidateSpellJson.ps1`, `NewSpellsExpansion/build_assets.ps1`, and
`python tests/ValidateBlizzardAssets.py`. Build/run `BlizzardMathTests.vcxproj`
and `AnimationCatalogTests.vcxproj`: 28 arithmetic/target-boundary checks and
14 animation preservation/capacity checks.

The native probe is compiled only with `/p:BlizzardNativeProbe=true` for both
DLLs. Run it only in an isolated copy of the game, then invoke:

```powershell
tests\ProbeBlizzardStartup.ps1 -GameDirectory '<isolated game>' -RequireNativeProbe
```

It uses synthetic armies on the actual game thread and engine functions,
including installed resistance/damage hooks. The probe passed 27,490 checks:
all mastery/recast combinations; native queue/count insertion and removal;
speed floors and Haste/Slow interactions; death-call veto and resurrection
status; consecutive battle state; status-only ERM; every legal center/target
pair; double-hex deduplication, friendly fire and slot 20; actual damage hook;
positive AI selection, slowing value, existing penalty, immunity, partial
resistance, lethal damage and collateral veto. Repeated AI evaluation preserves
the entire synthetic combat-manager memory image and makes zero native RNG
calls. The probe export and synthetic armies are absent from production builds.

Startup probes check both expansion-enabled and core-only catalogs, all 18
core definitions, Reinforcements, Remove Obstacle and the owned animation
binding. Asset tests compare all eight PNG payloads and the animation DEF
byte-for-byte with their source files. PE checks cover x86, XP subsystem and
static-runtime imports; deployed files are compared with the package.

Remaining manual checks: full animated hero/creature casts in real battles,
statistical resistance outcomes with damage and slow together, native death
and resurrection animations, actual Cure/Dispel casts, spellbook/editor and
creature-information visuals, save/load availability on a real map, and an XP
VM smoke test. Synthetic native tests do not substitute for those UI/battle
and legacy-OS checks.

Baseline source and installed-file backups are under
`backups/20260907-165145-before-blizzard-97`. Debugging used a disposable copy
at `work/blizzard-runtime-sandbox`; its files and test DLLs are not shipped.
