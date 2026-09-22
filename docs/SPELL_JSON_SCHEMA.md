# New Spells JSON and external-provider schema

New Spells uses ERA's merged `Lang/*.json` data as its spell-definition source.
The core package owns IDs through 95. External provider mods may declare fixed
IDs 96 through 199, a declaration becomes active only when a matching native
provider registers through `NewSpellsProviderApi.h`. A mod without a DLL can
instead declare a data spell with `NewSpells.DataSpells.<id>`, see
`DATA_SPELLS.md`.

## Standard spell fields

Each defined spell uses the JSON Overrides-compatible namespace
`era.spells.<id>`:

```json
{
  "era": {
    "spells": {
      "96": {
        "type": "0",
        "soundName": "Disguise.wav",
        "animationIndex": "36",
        "flags": "2",
        "name": "Reinforcements",
        "shortName": "Reinforcements",
        "level": "3",
        "school": "2",
        "spEffect": "0",
        "manaCost": { "0": "15", "1": "14", "2": "13", "3": "12" },
        "baseValue": { "0": "0", "1": "0", "2": "0", "3": "0" },
        "chanceToGet": {
          "0": "2", "1": "2", "2": "2", "3": "4", "4": "2",
          "5": "2", "6": "2", "7": "0", "8": "2"
        },
        "aiValue": { "0": "1", "1": "1", "2": "1", "3": "1" },
        "description": {
          "0": "...", "1": "...", "2": "...", "3": "..."
        }
      }
    }
  }
}
```

`school` is the native bitmask: Air `1`, Fire `2`, Water `4`, Earth `8`, or a
bitwise combination. `flags` is the native spell-record bitmask. In particular,
bit `0x8` (`SF_CREATURE_SPELL`) marks a creature-only record. No parallel
hard-coded creature-spell list is used.

The New Spells DLL reads these fields itself through ERA's translation API.
`ERA_JsonOverrides` may therefore be installed or absent; when present, both
plugins consume the same merged keys.

## Private New Spells fields

Mechanics that are not part of the generic spell record use
`NewSpells.Spells.<id>`:

- `animationKey`: logical name of a dynamically appended New Spells animation.
- `maxDurationRounds`: upper duration limit for a timed spell.
- `requiresBattleBetweenCasts`: Mobility's battle requirement.
- `permanentCasualties`: Incineration's non-resurrectable casualties.
- `temporarySpeedPenalty`: Explosion's one-round speed penalty.

## External spell declaration

An external provider places its normal record under `era.spells.<id>` and a
technical declaration under `NewSpells.ExternalSpells.<id>`:

The JSON document must have a provider-unique filename ending in
`NewSpells.json`, such as `Lang/YourProvider.NewSpells.json`, with locale
overlays using the same leaf filename. `Lang/NewSpells.json` is reserved for
the core mod. Equal VFS-relative filenames are replacements in ERA, not a
multi-file merge, so reusing that name can hide all 18 built-in definitions.

```json
{
  "NewSpells": {
    "ExternalSpells": {
      "96": {
        "provider": "HD.Plugin.H3.NewSpellsExpansion",
        "spellKey": "Reinforcements",
        "kind": "adventure",
        "editorVisible": "1",
        "capabilities": "1"
      }
    }
  }
}
```

`provider` and `spellKey` must exactly match the registered descriptor. `kind`
is `adventure`, `combat`, or `hybrid`; it must agree with both the descriptor
capabilities and native spell flags. `capabilities` is mandatory, nonzero, and
must exactly equal the descriptor's decimal capability mask. `editorVisible`
controls discovery by the map-editor extension. A missing provider, duplicate
ID, malformed record, identity mismatch, or missing required callback leaves
that spell blank and disabled.

Combat and hybrid declarations may add `combatTargetMode` with one of
`targeted`, `area`, `global`, or `summon`. If omitted, the mode is inferred
from the canonical targeting flags. An explicit mode must agree with those
flags. Adventure-only declarations must omit this field. External adventure
AI is owned exclusively by `EvaluateAdventureAi`, so the native
`SF_AI_ADVENTUREMAP` flag is rejected for external records.

An external spell may also declare one primary relocated magic animation with
`animationKey`, `animationDef`, `animationName`, and `animationType`. Keys must
be namespaced by the provider. Extra spell-specific data and localized UI text
belong to the provider's own namespace, for example
`NewSpellsExpansion.Spells.Reinforcements`.

`NewSpells.Config.MaxSpellId` remains a backward-compatible physical-table
floor. New Spells automatically raises the boundary to the highest valid
external declaration. `FU(GetMaxSpellId)` remains the highest structurally
defined hero-spell ID and skips level-zero/undefined and `SF_CREATURE_SPELL`
records.

The sidecars reserve 256 records and every executable bound follows the live
spell count, so the configured maximum is spell ID `199` (200 active records,
the size of the WoG spell table). `SPELL_ID_CEILING.md` lists the limits that
were lifted and the two cosmetic ones that remain. ERA loads the Lang json
after the plugins, so New Spells reads `MaxSpellId` and the
declarations in its `OnAfterWoG` handler.

Custom dynamically relocated animations use `animationKey`, not a generic
physical `animationIndex`. Stable native animations `0..82` continue to use
`animationIndex`; numeric references to relocated entries are rejected because
their physical positions depend on the active provider set. External animation
declarations are capacity-checked before the relocation table is changed.

## External graphics

New Spells pads the four icon sheets to 256 frames when the game loads them,
so every ID has a transparent carrier frame. Provider packages supply
true-color PNG overrides at the standard HD/ERA paths:

- `spells.def`, `SpellScr.def`, and `SpellBon.def`: frame `<spellId>`.
- `SpellInt.def`: frame `<spellId + 1>`.

The offset is part of the provider contract and is not dynamically allocated.
Provider build tooling should validate dimensions and write these paths into a
`*_png_data.zip`; it must not rebuild the core DEF palette.

## Availability and learning

Map-editor and `UN:J0` bans remain dynamic generation/availability state. They
do not change whether a record is structurally a hero spell. A record is
learnable only when it is defined (non-empty name, level `1..5`) and does not
carry `SF_CREATURE_SPELL`.

The extended `HE:M`, common AddSpell, generated/equipped spell-scroll, and
artifact-grant paths all enforce that structural rule. Availability bans do
not make an otherwise valid hero record fail those structural checks.

The former INI `[Enabled Spells]`, `UI.QueueFix` and `CumulativeUnicornAura`
options exist as `NewSpells.Config.DisabledSpells`, `QueueFix` and
`CumulativeUnicornAura` (see `DATA_SPELLS.md`), the per-spell `<Enabled>` key
has no replacement.
