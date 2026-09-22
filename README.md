# New Spells for ERA

Maintained source and a packaged ERA build of Alexspl's New Spells 1.03 RC2.
The working tree was reconstructed from the supplied ERA adaptation and checked
against the subsequently recovered official SoD source and binary package.

## Repository contents

- `NewSpells/`: core plugin, including the 2.12.3 BM:G compatibility fix.
- `NewSpellsEditor/`: map-editor integration.
- `NewSpellsExpansion/`: external provider spells and their source assets.
- `sdk/`, `tests/`, and `tools/`: provider examples, regression tests, and build tools.
- `dist/`: package resources, translations, and metadata; generated plugins and
  release archives are excluded from Git.

Historical game copies, investigations, reference downloads, old builds, and
release ZIPs were moved outside this repository. Only the current package
folders remain in `dist/`. See [workspace organization](docs/REPOSITORY_LAYOUT.md).
New local build products remain ignored. See [publication notes](docs/PUBLISHING.md)
and [third-party notices](THIRD_PARTY_NOTICES.md) before publishing a release.

## Build

Open `NewSpells.sln` and build `Release|Win32`, or run MSBuild directly. The
host also needs the v143 C++ toolset and Windows SDK 10 for the debug-map
converter, plus Python 3 for the build helper. The release project uses the current ERA SDK and the static MSVC runtime (`/MT`).
The solution builds `Release/NewSpells.dll` for the game,
`Release/NewSpellsEditor.dll` for the ERA map editor, and
`Release/NewSpellsExpansion.dll` for the optional external spell pack. All
projects target Win32 with the XP-compatible `v141_xp` toolset.

`dist/New Spells` contains the ERA package resources and metadata. A source
checkout needs the built DLLs and debug maps added before installation; the
local packaged copy includes them. Its SND and original PAC
payloads come from the official 1.03 RC2 release. The spell-icon DEFs are now
rebuilt by `tools/build_spell_graphics.ps1`: Golden Touch's empty small icon is
fixed and each icon DEF has transparent carriers covering every external spell
ID from 96 through 126. Its DLL is built from this tree. Spell records and
localization share ERA's merged
`era.spells.<id>` JSON format, which the DLL reads directly whether or not
ERA_JsonOverrides is installed.

`dist/New Spells Expansion` is a separate installable provider pack. Its first
spell is Reinforcements at ID 96, implemented by `NewSpellsExpansion.dll` with
town selection, the stock garrison window, borrowed-stack tracking,
movement/daily limits, and backward-compatible savegame state. Blizzard is the
second spell, at ID 97: a level-5 Water area attack with permanent flat slowing
and combat AI. See `docs/BLIZZARD_PORT.md` for its native integration and tests.
Future pack spells use stable IDs beginning at 98. The Golden Touch list icon still uses
the cloudless golden-hand treatment requested for `SpellInt.def` frame 96.

The complete ERA 2.7 package supplied separately was also reconciled. Its icon,
localized readmes, and repaired Russian translation are retained. Its 8.5 KB
placeholder DLL is deliberately replaced by the rebuilt plugin.

## Map editor integration

Version 2.10.0 added the playable New Spells entries to the existing Map
Specifications > Spells checklist. The editor extension is packaged under
`EraEditor`; it reuses the stock owner-drawn, multi-column list, leaves all 70
vanilla entries on the original code path, and now discovers declarations from
active external provider mods.

Because the H3M spell-disable field is fixed at 70 bits, new-spell selections
are stored in a small versioned trailer after the H3M gzip member. Maps with all
new spells enabled remain byte-for-byte vanilla at the end of an editor save.
The game plugin reads the trailer before map initialization and applies those
restrictions through the same extended availability array used by generation,
shrines, scrolls, mage guilds, and ERM checks. See
`docs/MAP_EDITOR_NEW_SPELLS.md` for the recovered editor seams and format.

## BM:G compatibility (2.12.3)

Legacy `BM:G` commands retain the same ERA receiver behavior with New Spells
loaded, including negative indices, morale/luck, both raw operands, and
cross-stack accesses. Original mastery addresses are translated to relocated
storage by physical stack position. Native IDs 0–80 accept the original value
domain while keeping spell removal; IDs 81–126 retain New Spells semantics,
including disabled-provider checks. No script migrations are required. Invalid
raw memory accesses remain unsafe. See [the investigation and validation
report](docs/BM_G_LEGACY_FIELD_COMPAT.md).

## Main repair

The former ERA build compiled executable-owned `std::vector` and `std::bitset`
members with a newer incompatible MSVC layout. In particular, `army` became
0x538 bytes instead of the executable's 0x548-byte object. AI combat code asks
heroes3.exe to copy temporary `army` objects, so the engine wrote past each
allocation and later destroyed fields at different offsets. That is a direct,
repeatable mechanism for the reported AI-turn heap corruption.

The executable-facing containers are now explicit trivial 32-bit ABI views,
with compile-time checks for every critical size and offset. See
`docs/AI_TURN_CRASH_ROOT_CAUSE.md` for the complete diagnosis.

## Status

- Release/Win32 builds successfully with `/MT`.
- The current source has no remaining high-confidence crash or heap-corruption
  finding after independent static passes.
- WoG/ERA spell and animation table relocations are chained instead of blindly
  overwriting other mods' tables.
- Known Game Bug Fixes Extended, WoG, and current HD Mod hook collisions are
  avoided or fail closed after signature validation.
- The orphaned SoD custom-LOD globals were removed; ERA now owns resource
  loading entirely through the packaged PAC and VFS.
- Full in-game AI stress testing is still required; follow `docs/TESTING.md`.
- New Spells keeps 18 built-in spells and publishes a versioned provider ABI
  for external adventure, combat, status, creature/ERM, lifecycle, and AI
  callbacks at fixed IDs 96 through 126.
- Reinforcements Spell 2.3 has been extracted into New Spells Expansion as a
  native provider at ID 96 rather than replacing Disguise.
- The 18 built-ins and active external declarations appear in the stock map
  editor Spells tab and can be disabled per map without changing the vanilla
  70-bit mask.
- Version 2.11.0 removes the separate spell-definition INI and its private
  enable switches. Creature-only records are identified by the native spell
  flag, while map and `UN:J0` bans retain their normal availability role.
- Version 2.11.1 publishes a versioned, read-only battle-AI spell query through
  Patcher_x86 and gives the existing Fear melee guard at `0x422088` a dedicated
  owner so general AI plugins can coexist without duplicating that hook. See
  `docs/AI_INTEROP.md` for the exact packed ABI and failure rules.

## Provenance

- Original New Spells implementation: AlexSpl
- Official comparison baseline: New Spells 1.03 RC2 for SoD
- Original discussion: http://heroescommunity.com/viewthread.php3?TID=47171
- New Spells Expansion / Reinforcements Spell: daemon_n; idea by ShimmY and
  Master of puppets (MIT)
- ERA integration and maintenance: Archer30

Downloaded comparison archives are preserved outside the repository as described
in `docs/REPOSITORY_LAYOUT.md`; they are not required to build the plugin.

## Original mod credits

- Author: AlexSpl
- Graphics: Rolex
- ERA adaptation: sokiee, daemon_n
- Russian translation: Panda Bei, daemon_n
- Original mod languages: English, Russian
- Current ERA repair and maintenance: Archer30

Additional spell contributions: Szaman (Hour of Power and Golden Touch).
