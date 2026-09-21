# External spell provider ABI

New Spells 2.12 publishes a Win32 C registry through the Patcher variable
`HD.Plugin.H3.NewSpells.SpellProviderRegistry.v1`. The authoritative packed
structures and constants are in `NewSpells/NewSpellsProviderApi.h`.

## Registration

External plugins must require and load after New Spells. This dependency order
does not guarantee Windows DLL process-attach order across different ERA mod
directories. Providers therefore call `Era::ConnectEra` and try the registry
during `DLL_PROCESS_ATTACH`; if New Spells has not published it yet, they make
exactly one deferred attempt from the ERA `OnAfterWoG` event. `OnAfterWoG` is
the common native-plugin rendezvous and occurs before New Spells seals provider
registration in post-initialization.

The SDK's `examples/ProviderBootstrap.h` implements this sequence. Provider
projects must compile/link `NewSpells/ERA/era.cpp`. When using the helper, call
`ProviderBootstrapV1::Start` once and do not separately call
`Era::ConnectEra` or `Patcher::CreateInstance`; `Start` owns those operations.
It validates registry size, ABI version, capabilities, state, and callback
addresses, then submits one batch. The bootstrap never retries a batch after
`RegisterProviderBatch` was actually invoked and rejected or faulted; doing so
could contest its own IDs. It also consumes only one deferred attempt even if
`OnAfterWoG` fires again.

Registration is accepted only for fixed spell IDs 96..126 and only before New
Spells seals the registry. Keep the bootstrap object, batch, descriptors,
identity strings, and callback code in static/process-lifetime storage.

Each descriptor repeats the batch provider key and supplies a stable spell key,
capability/flag masks, and callbacks. New Spells copies the descriptor into
fixed process-lifetime storage; callback code must remain loaded. Duplicate
IDs or inconsistent batch identities fail closed. There is no unregister or
runtime ID allocation in ABI v1.

The matching JSON declaration must carry the same mandatory, nonzero
`capabilities` mask. Combat target shape is declared as `targeted`, `area`,
`global`, or `summon` (or inferred from matching native flags). Provider
summons use the neutral immediate native dispatch and must implement all summon
validation and state in their callbacks; they never enter the stock
elemental-only switch.

All structures use `#pragma pack(push, 4)`, fixed-width integers, `__stdcall`
callbacks, and borrowed opaque pointers. Callbacks run synchronously on the
game thread. A provider must not retain context pointers after the callback.

## Callback behavior

- Adventure callbacks validate and execute a cast. On `COMMITTED`, New Spells
  deducts hero mana and plays the configured sound exactly once. `DENIED`,
  `CANCELLED`, and `UNSUPPORTED` consume nothing.
- Combat target and cast callbacks receive caster source, side, mastery,
  power, target hex, and borrowed hero/stack/manager pointers. A committed
  cast returns to the native cast epilogue; providers must not charge hero mana.
- Status callbacks are invoked for apply, round, remove, cure, and dispel
  transitions represented by the live spell-influence table. Cure and Dispel
  callbacks run before native removal: `DENIED` or `CANCELLED` vetoes removal,
  `COMMITTED` allows it, and `UNSUPPORTED` disables that provider spell before
  native cleanup continues. A committed Cure or Dispel is then followed by the
  separate `OnStatusRemove` notification.
- Battle lifecycle callbacks receive start/end events. Provider-specific save
  data and new-day state remain owned by the provider through normal ERA
  handlers.
- Creature and ERM casts are dispatched only when their explicit capability is
  present.
- AI callbacks return a score/target decision in their mutable context.
  Human-only providers are never invoked from AI processing. ABI v1 combat-AI
  callbacks participate in the visible/tactical battle planner; external IDs
  are deliberately excluded from the stock Quick Combat simulator because its
  hard-coded effect buckets only understand native spell mechanics.

For a registered external spell, `UNSUPPORTED` is fail-closed rather than an
instruction to run an unrelated stock spell implementation.

## Data and editor contract

The DLL descriptor is only the behavior half of a spell. The active provider
mod must also supply matching `era.spells.<id>` and
`NewSpells.ExternalSpells.<id>` records as documented in
`SPELL_JSON_SCHEMA.md`. Store them in a provider-unique
`Lang/*NewSpells.json` file; the exact core filename `Lang/NewSpells.json` is
reserved because equal VFS paths replace rather than merge. The map editor
discovers declarations from active mod folders and preserves all unknown bits
in the existing 128-bit map trailer.

`New Spells Expansion` is the reference provider. It registers
`HD.Plugin.H3.NewSpellsExpansion / Reinforcements / 96` in one batch and keeps
the spell's persistent state in the existing `NewSpells.Reinforcements`
savegame section.
