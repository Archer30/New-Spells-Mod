# Reinforcements graphics provenance

These PNGs and the retained original source ZIP come from Reinforcements Spell
2.3 by daemon_n (idea by ShimmY and Master of puppets), distributed under the
MIT license copied to `dist/New Spells/LICENSE-Reinforcements-Spell.txt`.

The renamed files describe their New Spells destinations:

- `spells-96.png` and `SpellScr-96.png`: spell ID/frame 96.
- `SpellInt-97.png`: the DEF uses a leading structural frame, so spell ID 96
  is frame 97.
- `SpellBon-96.png`: spell ID/frame 96 for consumers that expose that DEF.

The donor archive also contained a `Valspells.def` override. It is retained
only inside the untouched provenance ZIP; New Spells Expansion does not ship
or reference it because no current base-game, New Spells, map-editor, or
TrainerX resource consumes that DEF.

Run `tools/build_spell_graphics.ps1` to rebuild the binary DEF carrier frames
and `dist/New Spells/Data/NewSpells_png_data.zip`.
