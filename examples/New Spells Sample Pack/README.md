# New Spells Sample Pack

Fortitude (spell 150) is the built-in Toughness spell taken apart and rebuilt as a data spell:
no code, one JSON file per language, the icons, the animation and three script hooks. Copy the
folder to `Mods/`, list it below New Spells in `Mods/list.txt`, and the spell appears in mage
guilds, the map editor's spell page and the AI's choices.

```
mod.json                                   requires and loads after New Spells
Lang/SamplePack.NewSpells.json             era.spells.150 (the record) + NewSpells.DataSpells.150 (the kind)
Lang/ru/SamplePack.NewSpells.json          translated name and descriptions
Lang/zh/SamplePack.NewSpells.json
Data/Defs/SpellInt.def/0_151.png           spell book icon (this sheet has a leading null frame: id + 1)
Data/Defs/spells.def/0_150.png             large icon
Data/Defs/SpellScr.def/0_150.png           scroll icon
Data/NewSpells/Files/SmpTough.def          the animation, served to the game without any archive
Data/s/new spells sample pack.erm          the three script hooks
```

The data record says everything the engine needs to know:

```json
"kind": "Enchantment",
"modHealth": { "0": "130", "1": "130", "2": "140", "3": "150" },
"immuneSiegeWeapons": "1",
"animationDef": "SmpTough.def", "animationName": "Fortitude", "animationType": "1",
"scriptOnCast": "SamplePack_OnCast", "scriptOnApply": "SamplePack_OnApply", "scriptOnRemove": "SamplePack_OnRemove"
```

Things to try:

- Change `modHealth` or add `modAttack.<m>`, `modDefense.<m>`, `modSpeed.<m>`.
- Remove the 0x40 bit from `flags` to drop the expert mass version.
- Replace the PNG icons, the frames of any spell id are addressable, see `docs/DATA_SPELLS.md`.
- Delete `SmpTough.def` and add `animationFrames`, `animationWidth`, `animationHeight` plus
  `Data/Defs/SmpTough.def/0_<n>.png` frames, New Spells then generates the def.
- Put a message into the script hooks with `!!IF:M^...^`.

`sdk/examples/FortitudeProvider` is the same spell as a provider DLL (id 151), for the cases
where a spell needs code.
