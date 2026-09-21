# New Spells for ERA

This package ports Alexspl's New Spells 1.03 RC2 to current ERA and fixes the
known AI-turn memory-corruption path in the previous ERA build. Version 2.12.3
keeps its 18 built-in spells at IDs through 95 and adds a versioned provider
framework for separately packaged spells at IDs 96 through 126.

Version 2.12.3 restores legacy `BM:G` field compatibility, including ACM's
morale/luck commands, negative indices, and aliases into original mastery
storage. Both operands retain ERA's address, value, and operation behavior;
native spell operands no longer acquire extra value restrictions. IDs 81–126
keep New Spells semantics even when a provider is disabled. No ERM script
migration is required. Original invalid memory accesses remain unsafe.

The 2.12.2 integration update enables external combat callbacks with an expanded
spell catalog and preserves the installed More Anims animation table when
appending custom animations. It supports Blizzard (97) in New Spells Expansion
1.1.0. The public provider ABI and Quick Combat policy remain unchanged.

Version 2.10.0 also places the playable added spells in the existing map
editor Map Specifications > Spells tab and supports per-map disabling.
Version 2.11.0 moves every spell definition to ERA's merged JSON format and
uses the native creature-spell flag to distinguish non-hero records.
Version 2.11.1 adds a versioned read-only interface for general AI plugins to
query live spellbook, availability, mastery, mana-cost, and current-mana data.
It also gives the existing Fear melee guard a dedicated Patcher owner so a
cooperating AI plugin can avoid installing the same guard twice. Version
2.12.0 lets active external spell providers add their localized records,
graphics, map-editor entries, mechanics, and AI callbacks without installing
competing hooks in the central New Spells dispatch paths.
Version 2.12.1 isolates provider JSON filenames so an expansion cannot replace
the core catalog through ERA's VFS, rejects incomplete spell records
consistently, and lets the map editor discover provider-unique
`Lang/*NewSpells.json` files.

## Requirements

- Heroes III ERA 3.9.24 or newer (tested against the 3.9.31 runtime layout)
- WoG loaded before this mod

## Install

Copy the `New Spells` directory into the game's `Mods` directory. Put
`New Spells` after `WoG` and after `Advanced Classes Mod` in `Mods/list.txt`.
Lower lines in that file have higher resource priority.

Advanced Classes Mod contains a small compatibility placeholder named
`NewSpells.dll`. Do not delete or overwrite it. The dedicated New Spells mod
must load later so ERA selects this package's real plugin.

Install `New Spells Expansion` after this mod to add Reinforcements at spell
ID 96. The legacy ERM `Reinforcements Spell` mod remains a different,
incompatible implementation and should not be enabled with the expansion.

The bundled `EraEditor/NewSpellsEditor.dll` is loaded automatically by ERA's
map-editor extension. No editor executable replacement is required.

There is no `NewSpells.ini`. Spell records and text are defined under
`era.spells.<id>` in `Lang/NewSpells.json`; the DLL reads those keys directly,
so ERA_JsonOverrides is optional. External packages declare their identity and
kind under `NewSpells.ExternalSpells.<id>` and register matching native
callbacks through the public provider ABI. Each provider must use a unique
filename ending in `NewSpells.json` (for example
`Lang/YourProvider.NewSpells.json`); the exact core path is reserved. New
Spells no longer supplies the historical QueueFix or Cumulative Unicorn Aura
features.

Exit the game normally after testing. A forced termination may make ERA prefix
the newest mod with `*` in `Mods/list.txt`, disabling it for the next launch.

## External spells

The four packaged spell-icon DEFs cover every supported spell ID through 126.
IDs 96-126 use transparent carrier frames until an active provider supplies
its own true-color frame overrides. Golden Touch's formerly empty
`SpellInt.def` frame now uses
its existing golden-hand artwork without the tan cloud/oval backdrop.

External spell records are activated only when their JSON identity matches a
registered provider. Missing, malformed, duplicate, or incompatible providers
leave the corresponding record blank and disabled. The provider API is a
packed Win32 C ABI and includes adventure, combat, status lifecycle, creature,
ERM, and AI dispatch callbacks.

## Map editor

Open Map Specifications > Spells to find the 18 built-in New Spells entries
and any valid declarations discovered in active external mods. Checked means
allowed; clear an entry to disable it for that map. OK applies the changes,
while Cancel discards them normally.

The original H3M field remains untouched because it has room for exactly 70
spell flags. The editor stores only the added-spell choices in a checksummed,
versioned trailer after the map's gzip data. If every added spell is enabled,
the trailer is removed. The game consumes the extension before spell
generation and preserves it in savegames. This first implementation applies to
standalone H3M maps; campaign-contained maps do not carry this external trailer.

Map-editor and `UN:J0` bans affect generation and availability normally; they
do not turn a defined hero spell into a creature spell. Creature-only records
use native spell flag `0x8` and cannot be assigned through the extended
`HE:M`, common AddSpell, or spell-scroll paths.

## Important verification

First test without Advanced Classes Mod, then with the full mod stack. Exercise
both visible AI battles and Quick Combat and advance a seven-AI-player test map
for at least 50-100 turns. If a crash occurs, preserve `Debug/Era/log.txt`,
`x86 patches.txt`, `pe modules.txt`, `mod list.txt`, the exception context, and
the copied crash save before starting another run.

## Credits

Original design and SoD implementation: Alexspl. This package retains the
official 1.03 RC2 resources and English text while updating the ERA integration.
Additional spell work and packaging credits are preserved for Rolex, Szaman,
and Sokiee. ERA repair, integration, and maintenance: Archer30.
