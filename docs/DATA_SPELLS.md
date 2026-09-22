# Data spells, script events and pack resources

A data spell is a battle spell that a mod defines with JSON records only. New Spells serves it
through the built-in provider `HD.Plugin.H3.NewSpells.Data`, so it goes through the same
validation, map-editor discovery, save-game and dispatch paths as a spell registered by an
external provider DLL (see `EXTERNAL_SPELL_PROVIDERS.md`). No C++ is needed. Scripts and
plugins can react to spell events, and a pack can ship its icons, animation and sound without
repacking any archive.

`examples/New Spells Sample Pack` is a complete pack: the built-in Toughness spell rebuilt as
data spell 150 ("Fortitude") with its icons, animation, three languages and script hooks.
`sdk/examples/FortitudeProvider` is the same spell written as a provider DLL.

## Records

A pack declares a spell id in the range 96..199 with two records in a file named
`Lang/<Pack>.NewSpells.json` (the exact name `Lang/NewSpells.json` belongs to the core):

- `era.spells.<id>`: the standard spell record documented in `SPELL_JSON_SCHEMA.md`. It must
  describe a battle spell (`flags` has `SF_BATTLE_SPELL` 0x1 and not `SF_MAP_SPELL` 0x2).
- `NewSpells.DataSpells.<id>`: the data record below. An id that also carries a
  `NewSpells.ExternalSpells.<id>` declaration belongs to that provider, the data record is
  then ignored and logged.

Every value is a JSON string, as elsewhere in ERA language files.

| Key | Meaning |
|---|---|
| `kind` | Required. `Damage`, `AreaDamage`, `Enchantment`, `Summon` or `Custom`. |
| `spellKey` | Stable name of the spell (letters, digits, `.`, `_`, `-`). Default `Spell<id>`. |
| `editorVisible` | `1` (default) lists the spell in the map editor's spell page, `0` hides it. |
| `combatTargetMode` | Optional `targeted`, `area`, `global` or `summon`, it must agree with the flags. |
| `animationDef`, `animationName`, `animationType` | A magic animation of the pack: def name, a short name and the type (`1` on the target stack, `257` the Fear style). Without them the spell uses `animationIndex` of the standard record (a native animation 0..82). |
| `animationFrames`, `animationWidth`, `animationHeight` | With `animationDef`: the def is generated blank with this many frames and this size, the frames come from `Data/Defs/<animationDef>/0_<n>.png`. |
| `modAttack.<m>`, `modDefense.<m>` | Enchantment: attack and defense deltas per mastery `0`..`3` (none, basic, advanced, expert). A stat never drops below 0. |
| `modSpeed.<m>`, `modHealth.<m>` | Enchantment: percent per mastery, `100` = unchanged. Health multiplies into the same chain as Age, Toughness and Hour of Power. |
| `cancels` | Enchantment: spell ids removed from the target when this one lands, comma separated. |
| `immuneUndead`, `immuneNonLiving`, `immuneSiegeWeapons`, `immuneFireImmune` | `1` makes such stacks immune. The engine's own rules (mind spells, dragons, resistances) still apply. |
| `immuneCreatures` | Creature ids that are immune, comma separated. |
| `summonCreature` | Summon: the creature id. Required for that kind. |
| `summonExclusive` | Summon: `1` (default) locks the side to this creature like the elementals, `0` allows mixing. |
| `creatureMastery`, `creaturePower` | A creature ability that casts the spell uses this mastery (default 0) and power (default 3). |
| `scriptOnCast`, `scriptOnApply`, `scriptOnRound`, `scriptOnRemove` | ERM named functions called for this spell only (see Events). |

### How a kind maps onto the engine

| Kind | Targeting | Cast | Status | AI |
|---|---|---|---|---|
| `Damage` | one stack (`SF_SINGLE_TARGET`) | the engine's damage case: damage = power * `spEffect` + `baseValue.<m>` | none | the engine's damage evaluator |
| `AreaDamage` | a hex (`SF_TARGET_ANYWHERE`) | the engine's area damage with the Fireball shape | none | the engine's damage evaluator |
| `Enchantment` | one stack, all friendly stacks at expert with `SF_EXPERT_MASS_VERSION` 0x40 | the engine's generic enchantment case, duration = power | the stat modifiers above, removed by Dispel, Cure, Anti-Magic and death | attack through the engine's skill valuation, the rest as fractions of the stack's combat value |
| `Summon` | none (`SF_AI_CREATURES`) | the elemental summon path with the pack creature, count = `baseValue.<m>` * power | none | creature AI value times the count |
| `Custom` | one stack or a hex by flags | nothing, scripts and plugins act on the events | none | none |

Suggested `flags` values: Damage 33297, AreaDamage 33409, Enchantment 266261 (add 64 for the
expert mass version), Summon 524289. `type` is `1` for a helpful spell, `-1` for a harmful one
and `0` otherwise.

## Events

New Spells fires three ERA events for every spell id 70 and up. Plugins receive the structs
of `NewSpells/NsEvents.h` through `Era::RegisterHandler`, scripts receive named function
calls with the same values in `x1`..`x6`. The core ships empty triggers for the global names
in `Data/s/new spells.erm`, a pack adds its own triggers with the same names.

| Event | ERM function | Arguments |
|---|---|---|
| `NewSpells.OnBattleCast` | `NewSpells_OnBattleCast` | spell, side, target hex, mastery, power, creature cast flag |
| `NewSpells.OnStackSpell` | `NewSpells_OnStackSpell` | spell, side, stack index, mastery, applied flag, hero id or -1 |
| `NewSpells.OnAdventureCast` | `NewSpells_OnAdventureCast` | spell, hero id, mastery |

The battle cast event fires before the effect, the stack event when a timed spell is applied to
or removed from a real battle stack (never for the AI's simulated copies), and the adventure
event when a hero casts an adventure spell of id 70 and up. The per-spell `scriptOn*` functions
of a data spell get the same arguments, `scriptOnRound` gets spell, side, stack index, mastery
and the remaining duration each round. `NewSpells.Config.ScriptEvents` = `0` switches the
global ERM calls off, the ERA events and the per-spell functions stay.

## Resources without repacking

ERA draws `Data/Defs/<def>/<group>_<frame>.png` of any mod over the frames of a def. New
Spells pads the four spell icon sheets to 256 frames when the game loads them, so every id has
a frame slot:

- `SpellInt.def`: frame `<id + 1>` (the sheet has a leading null frame), 48x36
- `spells.def`: frame `<id>`, 78x65
- `SpellScr.def`: frame `<id>`, 83x61
- `SpellBon.def`: frame `<id>`, 58x64

Files that must come from an archive (`.def`, `.pcx`, `.msk`) can instead sit in
`Data/NewSpells/Files` of any mod, New Spells serves them to the game's archive reader. Names
have at most 15 characters. An animation that exists only as PNG frames needs no def file at
all: `animationFrames`, `animationWidth` and `animationHeight` make New Spells generate a blank
one. Sounds (`soundName`) are read by the game from `Data` directly.

`tools/lodtool.py` and `tools/deftool.py` (in the NewSpellsV3 folder of the original work) list,
extract, pack and pad archives and defs for authors who prefer packing.

## Options

`NewSpells.Config.*` keys in any Lang json:

| Key | Default | Meaning |
|---|---|---|
| `MaxSpellId` | `95` | Floor of the physical spell count. Declarations raise it automatically. |
| `ScriptEvents` | `1` | Call the global ERM event functions. |
| `CumulativeUnicornAura` | `0` | Every unicorn aura source multiplies the resistance chance by 0.8, not only the first. |
| `QueueFix` | `0` | HD mod battle queue shows the slowed speed of Slow, Disease, Fear, Explosion and data enchantments (needs `HD_SOD.dll` of the tested layout). |
| `DisabledSpells` | | Spell ids disabled on every map, comma separated. |

## Limits

- Ids 96..199 are addressable, the WoG spell table has 200 records.
- A stack shows the tint of active spells with ids below 162 only (the stack drawing loop).
- The HD mod battle replay stores per-stack durations with its own layout, ids 162 and up are
  not replayed.
- Damage over time is an `Enchantment` with a `scriptOnRound` function, there is no built-in
  damage kind per round yet.
