# Runtime test plan

## Install order

Install `dist/New Spells` as `Mods/New Spells` and place it after WoG and after
Advanced Classes Mod in `Mods/list.txt`. Do not replace Advanced Classes Mod's
small placeholder `NewSpells.dll`; the later dedicated mod should shadow it.
For Reinforcements tests, install `dist/New Spells Expansion` after New Spells.
Disable the legacy `Reinforcements Spell` mod, which is incompatible with the
expansion. Era Erm Framework and `NewSpells.ini` are no longer required. Test
once with ERA_JsonOverrides active and once without it; the same merged
translation keys must be used in both cases. The core owns
`Lang/NewSpells.json`; every provider must retain its unique
`Lang/*NewSpells.json` filename.

## Spell ceiling probe

Prepare a disposable ERA 3.9.24+ game copy at `work/era3924`, with WoG and
the current `dist/New Spells` resources installed. The probe may run only under
the repository's `work/` directory. It reads `Debug/Era/log.txt` after closing
the game because ERA writes that log at exit.

Build and run the repeatable regression fixture:

```powershell
python tools/build_release.py NewSpells/NewSpells.vcxproj work/ceiling-probe-build --ceiling-native-probe
& tests\InvokeSpellRegression.ps1 -GameDirectory work\era3924
```

The runner creates a temporary mod from the checked-in sample pack, adding a
copy of its enchantment at ID 199 beside ID 150. It uses a fixed mod list and
requires 200 spell slots, both active enchantments, and an explicit fixture
coverage marker from the probe. The DLL/debug map and original mod list are
restored even on failure; the generated fixture is archived under `backups/`.
Probe DLLs and fixture mods must never be packaged.

The named regression groups cover mage-guild hook targets, duration clearing
without queue corruption, temporary-stack lifetime (including 96 simultaneous
copies and repeated address reuse), disabled flags across IDs 139/140, and
the actual AI query plus effect application, cancellation and expiration.
Failure logs include the group, condition and source line.

For checks against a different installed configuration, use the lower-level
runner and specify its expected counts:

```powershell
& tests\ProbeSpellCeiling.ps1 -GameDirectory work\era3924 -ExpectedSpellCount 96
& tests\ProbeSpellCeiling.ps1 -GameDirectory work\era3924 -ExpectedSpellCount 151 -ExpectedDataSpells 1
```

The second command expects `examples/New Spells Sample Pack` installed below
New Spells in `Mods/list.txt`. AI/effect tests in this mode cover only the
installed active enchantments; the log reports that coverage explicitly.

On ERA 3.9.10 the core cannot load because its ERA binding calls exports that
do not exist there (`CreatePlugin`, `WriteLog`, `trStatic`). In `Mods/list.txt`,
a mod listed lower wins for files of the same name. Advanced Classes Mod ships
a placeholder `NewSpells.dll`, so New Spells must be listed below it.

## Startup verification

After reaching the adventure map, refresh ERA diagnostics and verify:

- `Debug/Era/mod list.txt`: New Spells has the intended VFS priority.
- `Debug/Era/pe modules.txt`: the rebuilt DLL is loaded, not the placeholder.
- `Debug/Era/x86 patches.txt`: New Spells patches have no unexpected overlaps.
- `Debug/Era/log.txt`: no NewSpells signature or relocation warnings.

Exit normally after a test. Force-terminating Heroes III can make ERA prefix the
most recently enabled mod with `*` in `Mods/list.txt` as crash-loop protection;
remove that prefix before the next New Spells run if it occurs.

## BM:G compatibility regression (2.12.3)

See [BM:G compatibility](BM_G_LEGACY_FIELD_COMPAT.md) for the policy, reproducible
address/parser probes, results, and limitations. The disposable-copy probes
compare original ERA receiver behavior with New Spells runtime changes disabled
and enabled. Their test DLLs must never be packaged.

After those checks, replay actual ACM melee/ranged attacks with positive,
zero, and negative luck; Night Scouting casualty accounting; and Grand Manouvre
morale save/restore. Include spell removal and active/disabled providers.
Command-sequence probes have passed; full battle replay remains outstanding.

## Functional smoke test

1. With the core alone, verify its 18 names, abbreviations, levels, icons, and
   four mastery descriptions. Enable the expansion and repeat for ID 96.
2. Cast every spell and verify its animation and sound.
3. Exercise shrines, scrolls, RMG spell generation, disabled-spell rules,
   save/load, battle replay, and return to the main menu.
4. Target living, undead, immune, war-machine, summoned, cloned, nearly empty,
   killed, and resurrected stacks where the spell rules permit it.
5. Exercise Anti-Magic, Cure/Dispel, expiration, stack death, and resurrection.

## Map editor round-trip test

1. Start the ERA map editor with New Spells active, create a new standalone H3M
   map, and open Map Specifications > Spells.
2. Confirm the original 70 entries are unchanged and the 18 built-ins appear
   in the same stock multi-column checklist. Enable New Spells Expansion and
   confirm Reinforcements is dynamically appended at ID 96.
3. Clear Golden Touch and Reinforcements, press Cancel, reopen the page, and
   confirm both remain checked.
4. Clear them again, press OK, save the map, close it, reopen it, and confirm
   both remain cleared while adjacent spells remain checked.
5. Inspect the saved file for the 44-byte `NSMAPSPELLS1` trailer. Re-enable all
   added spells, save again, and confirm the trailer is removed.
6. Start the saved map in ERA and verify the cleared spells do not appear in
   mage guilds, shrines, or scroll generation. Confirm `!!UN:J0` reports the
   same disabled state, then save/reload and repeat the checks.

## Native creature and learning guards

1. Verify `FU(GetMaxSpellId)` returns 95 with only New Spells and 96 with New
   Spells Expansion, never the physical boundary or creature-spell range.
2. Through extended `HE:M`, query and try to assign a defined hero spell, a
   native creature spell such as ID 70, and a blank/reserved extended record.
   The hero spell must work; creature and blank records must be rejected.
3. Repeat through a common AddSpell caller, a generated scroll, an equipped
   scroll, and tome/hat spell refresh. Creature and blank records must never
   enter the hero spellbook.
4. Disable an ordinary spell through the map editor and separately through
   `!!UN:J0`. TrainerX must still list every ID through 69 as before and must
   list structurally valid extended hero spells; it must skip extended records
   with native flag `0x8` or an undefined level/name.
5. Change an experimental extended record's native flags with `!!SS` and
   confirm TrainerX follows bit `0x8`, without a hard-coded creature-ID list.

## Reinforcements smoke test

1. With New Spells Expansion disabled, confirm ID 96 is blank, unavailable,
   unlearnable, absent from the editor, and rendered by transparent carriers.
   Enable the expansion and confirm Disguise remains unchanged while
   Reinforcements appears at ID 96 with Fire level-3 metadata and correct icons.
2. At no/Basic Fire Magic, confirm the closest eligible town is chosen; at
   Advanced/Expert, confirm the native paged chooser can select any eligible
   town without invoking or changing Town Portal.
3. Transfer creatures from an unoccupied garrison, a visiting hero, and a
   garrisoned hero. Verify the exchange uses the stock Heroes III garrison
   window and a garrison hero's final stack cannot be removed.
4. Move a town stack to the lower row and back to the upper row; this must be
   allowed. Move it down again, merge it with a stack that originally belonged
   to the caster, then verify the mixed stack cannot move to the upper row.
   Also verify an untouched original hero stack can never move upward.
5. Cancel the town chooser, close the garrison window without transferring,
   and return every borrowed stack before closing; each case must consume no
   mana, movement, or daily use. On success verify movement deductions are
   300/200/200/100 at
   None/Basic/Advanced/Expert, mana is spent once, and daily limits are
   1/2/3/4 successful casts. None through Advanced require at least 300
   movement before casting; Expert requires at least 200.
6. Verify the system split control preserves the same rule: a pure borrowed
   split remains returnable, while any split produced from a mixed stack is
   not returnable. Check creature info and WoG stack experience as well.
7. Confirm the title contains the selected town's real name, with no pointer
   glyphs or replacement boxes.
8. Advance a day and verify the per-hero limit resets. Save/load before and
   after a successful cast and verify the army and spell table remain valid.
9. Verify `SpellInt.def` frame 96 shows Golden Touch without the tan cloud,
   frame 97 shows
   Reinforcements only while the expansion is active, `SpellScr.def` retains
   its final All Spells frame, and the legacy Reinforcements mod is rejected
   when New Spells Expansion is enabled.
10. Verify the rebuilt DEF layouts: `spells.def` and `SpellBon.def` have 127
    frames (spell IDs 0-126), `SpellInt.def` has 128 frames (including its
    one-frame offset), and `SpellScr.def` has 128 frames with All Spells still
    last. Confirm reserved
    spell IDs 96 and 126 render as transparent carriers with the core alone;
    the expansion must override only ID 96 without changing either DEF.

## External provider ABI test

1. Validate the packed sizes and offsets in `NewSpellsProviderApi.h`, then find
   `HD.Plugin.H3.NewSpells.SpellProviderRegistry.v1` after New Spells loads.
2. Register a non-shipped fixture across adventure, targeted, area, global,
   summon, damage, timed-status, creature, ERM, battle lifecycle, and AI paths.
3. Confirm adventure cancellation consumes no mana or sound and a committed
   cast consumes both exactly once. Confirm combat commits use the native cast
   epilogue and do not double-charge mana.
4. Exercise status apply, round, remove, cure, and dispel transitions through
   hero casting and `BM:G`, including death, resurrection, and replay.
5. Test missing DLL, wrong ABI/size, malformed JSON, identity mismatch,
   duplicate provider, duplicate ID, missing required callback, invalid custom
   animation, and a faulting fixture callback. Each slot must fail closed while
   unrelated built-in and external spells remain valid.
6. Remove an external provider after saving a map, open and save the map, then
   restore the provider. Its 128-bit trailer setting must survive the absence.

## AI regression test

1. Establish a baseline with ERA/WoG and New Spells but without Advanced Classes.
2. Use seven AI players whose heroes know the new spells and have ample mana.
3. Exercise both visible battles and Quick Combat/theoretical AI-vs-AI battles.
4. Advance at least 50-100 AI turns while forcing frequent battles.
5. Repeat with the full normal mod stack including New Spells Expansion, TUM
   Reborn, Amethyst, and ERA AI Improvements in their declared order.
6. Repeat with ERA_JsonOverrides enabled and disabled.

## AI interoperability and coexistence test

1. Build and run `tests/NewSpellsMapFormatTests.vcxproj`; it also compiles and
   exercises the exact packed-4 ABI layout from `NewSpellsAiInterop.h`.
2. Start once without a consumer plugin and confirm New Spells logs no
   `AI spell-query interop` or `Fear melee-guard ownership` failure.
3. With the General AI plugin loaded, inspect the Patcher variable
   `HD.Plugin.H3.NewSpells.AI.SpellQuery.v1`: its provider must report ABI 1,
   size 16, capability bit 0, and a non-null callback.
4. Query an attacker and defender hero for a known enabled spell, an unknown
   spell, a map-disabled spell, and an invalid/reserved ID. Compare mastery,
   mana cost, and current mana with the battle UI; include an opposing Pegasus
   stack to verify the effective cost changes through the live engine call.
5. Repeat with General AI before and after New Spells in plugin load order and
   with New Spells absent. The consumer must use the provider when present and
   preserve its stock-spell fallback when absent.
6. Confirm `x86 patches.txt` attributes `0x422088` only to
   `HD.Plugin.H3.NewSpells.FearMeleeGuard.v1`, while a feared active stack still
   cannot make a melee attack. The callback must not alter ESI.
7. In an isolated collision fixture, pre-create the provider variable and then
   the Fear owner name. New Spells must log each collision, leave the existing
   provider value untouched, and install no second `0x422088` guard.

On failure, preserve the exception context, screenshot, copied crash save,
`log.txt`, `x86 patches.txt`, `pe modules.txt`, `mod list.txt`, `Mods/list.txt`,
the core and provider `*NewSpells.json` documents, and byte-for-byte copies of
the tested DLLs before launching again.
