# External spell graphics

`spell-assets.json` is the package manifest. Each spell declares its fixed ID
and one source PNG for each supported DEF; frame numbers are deliberately not
accepted in the manifest. The builder derives the ERA override path
`Data/Defs/<def>/0_<frame>.png`: `spells.def`, `SpellScr.def`, and
`SpellBon.def` use the spell ID, while `SpellInt.def` uses `spellId + 1`.
This keeps future spell assets from silently targeting an
off-by-one frame.

Run `build_assets.ps1` from the project directory to validate the manifest and
rebuild `dist/New Spells Expansion/Data/NewSpellsExpansion_png_data.zip`.
It also writes `NewSpellsExpansion.pac` with the unchanged Blizzard DEF payload
under `NSEBLIZ.def`. The native `C18SPW0.def` resource is never overridden.

Blizzard uses frames 97 in `spells`, `SpellScr`, and `SpellBon`, and frame 98
in `SpellInt`. Its artwork and animation come from Blizzard Mod by daemon_n
with ShimmY. The original animation archive and readme are in `provenance/`.

The Reinforcements artwork comes from Reinforcements Spell 2.3 by daemon_n,
based on an idea by ShimmY and Master of puppets, and is retained under the MIT
license shipped with the mod.

`provenance/reinforcements_png_data.original.zip` is the unchanged donor
archive. Its original spell-4/offset-frame-5 paths are retained for provenance;
the remapped source PNGs in `source/` target external spell ID 96.
