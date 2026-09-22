# New Spells external-spell SDK (ABI v1)

This SDK lets a Win32 ERA plugin provide native mechanics for fixed spell IDs
96 through 126. New Spells remains the single owner of the spell tables and
central dispatch hooks; provider DLLs register callbacks and must not hook
those dispatch points themselves.

The public C ABI is `include/NewSpellsProviderApi.h`. The compiling core keeps
the canonical copy at `NewSpells/NewSpellsProviderApi.h`; the published SDK
copy must remain byte-identical. Run `tests/verify_header_sync.ps1` after every
ABI edit.

## Binary contract

- Build a PE32/Win32 DLL. The supplied projects use `v141_xp`, `/MT`, four-byte
  member alignment, and Windows subsystem/minimum OS 5.01.
- All public structures use fixed-width integer types, a four-byte packing
  boundary, a leading `size`, and `abiVersion == 1`.
- All callbacks are `__stdcall`. Do not pass C++ objects, STL containers,
  exceptions, allocators, references, or C++ `bool` across the DLL boundary.
- Engine pointers are opaque, borrowed, and valid only for the callback. Never
  retain or free them.
- Keep descriptor, batch, identity strings, and callback code alive for the
  lifetime of the DLL. New Spells copies a validated descriptor but callback
  addresses remain owned by the provider.
- Check every incoming context's `size`, `abiVersion`, and `spellId` before
  reading it. A larger `size` is forward-compatible; do not require equality.

Every callback returns exactly one `NewSpellsProviderResultV1` value:

- `UNSUPPORTED`: the requested path cannot be implemented safely. New Spells
  treats a required unsupported path as fail-closed; never fall through to an
  unrelated native spell.
- `DENIED`: validation failed and nothing was consumed.
- `CANCELLED`: the user or provider cancelled and nothing was consumed.
- `COMMITTED`: the provider-specific effect completed successfully.

For adventure spells, New Spells consumes mana and plays the standard sound
exactly once only after `CastAdventure` returns `COMMITTED`. For combat spells,
return to the normal New Spells combat-cast epilogue; it owns hero mana and
cast-count consumption. Providers own their spell-specific resources and
state. A provider that mutates game state must return `COMMITTED`, even if a
later nonessential operation fails, or must fully roll the mutation back.

## Registration

Declare that the provider requires and loads after `New Spells`, but do not
rely on that metadata for native DLL process-attach order. ERA may attach DLLs
from different mod directories in another order. `OnAfterWoG` is the native
plugin rendezvous: all providers have attached by then, while New Spells still
accepts provider registrations.

The supplied `examples/ProviderBootstrap.h` implements the required sequence.
When using it, call `ProviderBootstrapV1::Start` once and do not separately
call `Era::ConnectEra` or `Patcher::CreateInstance`; `Start` owns those steps.

1. During `DLL_PROCESS_ATTACH`, `Start` calls `Era::ConnectEra`, obtains
   Patcher, and creates a uniquely named `PatcherInstance`.
2. Try the validated
   `HD.Plugin.H3.NewSpells.SpellProviderRegistry.v1` immediately.
3. If the registry is not available yet, register one `OnAfterWoG` handler and
   make exactly one deferred attempt there. If an immediate registry call is
   rejected or faults, do not retry the same batch: a second submission could
   turn its IDs into duplicate/contested slots.
4. Submit every descriptor owned by the DLL in one `RegisterProviderBatch`
   call. Registration closes during New Spells post-initialization.

Compile and link `NewSpells/ERA/era.cpp` in the provider project; including
`era.h` alone does not define the ERA imports used by the bootstrap. Keep the
`ProviderBootstrapV1` object, batch, descriptor array, identity strings, and
callback code in static/process-lifetime storage. Call `Start` only from the
single provider bootstrap translation unit. The helper guards repeated calls
and repeated `OnAfterWoG` events, so at most one registry submission occurs.
After successful registration, provider mechanics and callbacks must remain
ready for the process lifetime.

Registration is all-or-nothing. Invalid identities, IDs outside 96-126,
duplicate/contested IDs, missing required callbacks, unsupported capabilities,
closed registration, or a callback fault leave the affected spell disabled.
IDs are persistent data and must never be allocated dynamically.

## Descriptor capabilities

Set only the bits your provider implements and supply every callback required
by those bits. `ADVENTURE_CAST` requires `CastAdventure`; its
`ValidateAdventure` callback is optional but may only accompany that bit.
`COMBAT_TARGET` and `COMBAT_CAST` describe target validation and full cast
execution. Status apply/round/remove and cure/dispel are independent.
Lifecycle, creature-cast, ERM-cast, combat-AI, and adventure-AI each have their
own bit.

Set `NEWSPELLS_PROVIDER_HUMAN_ONLY` for a modal or otherwise human-only spell
and leave both AI callbacks null. Never open a modal interface from an AI
callback. Combat AI returns its `score`, `targetHex`, and `castNow` through
`NewSpellsAiContextV1`; adventure evaluation uses the same outputs, and the
selected provider-defined adventure target is forwarded in
`NewSpellsAdventureContextV1::target` when the cast is dispatched.
Combat-AI callbacks participate in visible/tactical battles. ABI v1 excludes
external IDs from the stock Quick Combat simulator because its effect buckets
only understand native spell mechanics.

For Cure and Dispel, `DENIED` or `CANCELLED` vetoes native removal,
`COMMITTED` permits it, and `UNSUPPORTED` fails the spell closed before native
cleanup continues.

## Required JSON

Place the base file in the provider mod under a globally unique filename with
the `NewSpells.json` suffix, for example
`Lang/YourProvider.NewSpells.json`. Put language overlays under the identical
filename at `Lang/<language>/YourProvider.NewSpells.json`; translations should
override text but keep the technical identity unchanged. Do not use the
core's exact `Lang/NewSpells.json` path: ERA's VFS resolves equal relative
filenames to one winning file, which would hide either the core catalog or
another provider. The declaration shape is:

```json
{
  "NewSpells": {
    "ExternalSpells": {
      "97": {
        "provider": "HD.Plugin.H3.YourProvider",
        "spellKey": "YourStableSpellKey",
        "kind": "adventure",
        "editorVisible": "1",
        "capabilities": "1"
      }
    }
  }
}
```

`kind` is `adventure`, `combat`, or `hybrid` and must agree with the standard
spell flags and registered capabilities. `capabilities` is mandatory, nonzero,
and must be the decimal value of the exact descriptor mask. Also supply
the full standard record under `era.spells.<id>`: name, descriptions, level,
school, flags, mana, effect, AI value, sound, and all town probabilities.
Provider-specific text and mechanics config should live under a unique
namespace such as `YourProvider.Spells.<spellKey>`.

Combat declarations may set `combatTargetMode` to `targeted`, `area`,
`global`, or `summon`. When omitted, New Spells infers the mode from canonical
spell flags. An explicit value is validated and must describe those same flags:
single-target, area/target-anywhere, summon/AI-creatures, or no target-shape
flag for global. `ValidateCombatTarget` remains the provider's final gameplay
check.

External adventure records must not set native `SF_AI_ADVENTUREMAP`; their AI
entry point is exclusively the versioned `EvaluateAdventureAi` callback.

An optional primary custom animation uses flat declaration fields
`animationDef`, `animationName`, `animationType`, and the exact namespaced
`animationKey`. Icon overrides remain provider assets. For ID `n`, use frame
`n` in `spells.def`, `SpellScr.def`, and `SpellBon.def`, and frame `n + 1` in
the offset-based `SpellInt.def`.
Numeric `animationIndex` values are limited to stable native entries `0..82`;
relocated animations must be selected by their namespaced key.

## Included providers

- `examples/MinimalAdventureProvider` is a small human-only ID97 skeleton. Its
  cast callback intentionally returns `UNSUPPORTED` until the marked provider
  effect is implemented, so it cannot consume mana for a no-op.
- `tests/AllCallbacksProvider` is a non-shipped four-descriptor batch covering
  IDs 123-126 and the targeted, area, global, and summon dispatch modes. Across
  the batch it exercises every v1 callback and exports aggregate and per-spell
  counters. It deliberately performs no real spell mechanics and must never be
  placed in a release package.

Build both with:

```powershell
./sdk/build_examples.ps1 -Configuration Release
```

Outputs stay under `sdk/build`; the script does not deploy or modify a mod.
