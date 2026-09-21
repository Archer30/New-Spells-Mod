# Native map-editor support for New Spells

## Goal and compatibility boundary

The Heroes III map format stores its disabled-spell mask as exactly nine bytes
covering the original spell IDs. Expanding that field would shift every later
H3M field and make the map unreadable to the stock editor and game. Version
2.10.0 therefore preserves the original field and adds a separate, optional
extension for built-in IDs 71, 73, 75, and 81 through 95 plus declared
external spell IDs 96 through 126.

The extension is installed by `EraEditor/NewSpellsEditor.dll`. It validates the
live editor image structurally and by method/vtable signatures. If that profile
is not present or another non-chainable patch owns a seam, it remains inactive
without changing the editor.

## Recovered native seams

Both supplied databases, `h3maped.i64` and `_unleashed.exe.i64`, resolve the
same code and the corresponding executables are byte-identical for these
methods:

| Purpose | Preferred VA | Validation anchor |
| --- | ---: | --- |
| Spells page `OnInitDialog` | `0x47666E` | vtable `0x53C3E0` |
| Spells page `OnApply` | `0x47680C` | vtable `0x53C3E8` |
| spell check-change handler | `0x476860` | message map `0x53C304` |
| map document `OnNewDocument` | `0x45EF8D` | vtable `0x53A864` |
| map document `OnOpenDocument` | `0x45F3BB` | vtable `0x53A868` |
| map document `OnSaveDocument` | `0x45F3DE` | vtable `0x53A86C` |
| `CDocument::SetModifiedFlag` | `0x45ECE4` | exact method body |
| `CCheckListBox::SetCheck` | `0x515D35` | exact method body |
| `CCheckListBox::GetCheck` | `0x515DB0` | exact method body |

Resource dialog 345 contains one owner-drawn, multi-column `CCheckListBox`
with control ID 1937. Its embedded object begins at page offset 184 and its
window handle is at offset 212. The original initializer adds spell records
from the 70-entry table at `0x593508`, using record stride `0x88`, and stores
the spell ID as each list item's data.

The stock check-change handler indexes that 70-entry table directly. Passing an
appended ID to it would read outside the array. The editor DLL therefore
short-circuits built-in and dynamically discovered appended IDs. Every original
ID continues down the untouched native handler, including its school/group
constraints.

External entries are discovered from active, non-`*` entries in
`Mods/list.txt`. The editor enumerates each mod's base
`Lang/*NewSpells.json` documents and the same filenames in the selected
language folder, validates `NewSpells.ExternalSpells.<id>`, and combines each
localized `era.spells.<id>.name` with the built-in list. Provider-unique
filenames prevent ERA's VFS from replacing one spell catalog with another.
Duplicate or malformed external IDs are omitted rather than resolved by load
order.

`OnApply` is the transaction boundary. Added checkbox states are copied into
the extension state only after the native page accepts the change. Cancel does
not call this path, so it has normal stock semantics. A changed extension state
marks the current map document modified.

## H3M trailer format

The optional trailer is a packed 44-byte record at the physical end of the H3M
file, after its gzip member:

| Field | Bytes | Value |
| --- | ---: | --- |
| magic | 12 | `NSMAPSPELLS1` |
| format version | 4 | `1` |
| bit count | 4 | `128` |
| disabled bits | 16 | bit 1 means disabled |
| FNV-1a integrity value | 4 | all fields except this value |
| total size | 4 | `44` |

Only displayed spell bits are changed. The complete 128-bit state is retained,
including bits belonging to providers that are temporarily absent. Saving
replaces an existing valid trailer instead of stacking another one. Saving
with every bit clear removes the trailer entirely, leaving ordinary H3M bytes.
Invalid or partial trailing data is never trusted as New Spells metadata.

The game DLL reads this record before `game::LoadMap` performs spell setup and
adds the result to `Game+4`, the extended availability array already consumed
by New Spells generation and ERM support. The patched `!!UN:J0` receiver reads
and writes this exact array, so an editor-disabled New Spell is reported as
disabled by `!!UN:J0`, and a later `!!UN:J0` change updates the same runtime
state. The state is also stored in a versioned ERA savegame section so a saved
game does not depend on later edits to the source H3M.

This format currently covers standalone H3M files. Maps embedded inside a
campaign have no independent external H3M tail and are intentionally left on
the vanilla behavior until a campaign-container extension is designed.
