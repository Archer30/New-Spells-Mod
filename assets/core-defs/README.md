# Core DEF sources

`SpellBon.def` is a lossless extraction of the stock 70-frame resource from
`Data/H3sprite.lod`. The core graphics builder preserves its palette and all
original frame records, then appends carrier frames through spell ID 126.

Frames 96 through 126 are deliberately transparent. External providers supply
visible true-color overrides at `Data/Defs/SpellBon.def/0_<spellId>.png`.
