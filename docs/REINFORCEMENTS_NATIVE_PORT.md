# Reinforcements native port

> New Spells 2.12 moves this native implementation unchanged in behavior to
> the separately installable New Spells Expansion provider at spell ID 96.
> The core NewSpells.dll no longer contains Reinforcements code or data.

Version 2.9 moved the imported Reinforcements gameplay layer from ERM into
`NewSpells.dll`. Version 2.9.1 replaces its bespoke army-transfer window with
Heroes III's stock `TGarrisonWindow`. The standalone donor mod remains
incompatible because both mods claim the same spell and graphics.

## Native behavior

- None/Basic Fire Magic chooses the nearest eligible owned town.
- Advanced/Expert Fire Magic opens the paged native town-selection dialog.
- Army exchange runs in the system garrison window used for map garrisons and
  mine troops, including its normal split, stack-information, redraw, and
  WoG/HD command-chain behavior.
- A garrisoned hero's final stack remains protected by temporarily presenting
  the selected town as the town manager's current town.
- Mana, movement, daily mastery limits, successful-cast counting, and map
  redraws are owned by the DLL.
- Daily counts use the versioned `NewSpells.Reinforcements` ERA savegame
  section. Saves from the ERM-backed build migrate the donor associative
  counters when no native section is present.

The port removes the temporary Town Portal byte patches, hidden-hero/team
mutation, Town Portal selection hook, ERM function-event bridge, three
Reinforcements ERM files, and `reinforcements.pac`. The remaining
`new spells.erm` publishes the existing public spell-range helpers and does
not require Era Erm Framework.

## Reversible borrowed stacks

The lower row starts with one provenance tag per caster stack:

| Tag | Meaning | May move to the town row? |
|---|---|---|
| Empty | No stack | Not applicable |
| Hero origin | Present before the dialog opened | No |
| Town returnable | Moved or split from the town row in this dialog | Yes |
| Mixed | Contains both town-origin and hero-origin troops | No |

The hook follows stock merge, exchange, and split commands at
`townManager::DoCommand` (`VA 0x5D5010`). Moving a pure town stack between
lower slots keeps it returnable. Combining town-origin troops with a stack
that originally belonged to the caster marks the result mixed. Splitting a
mixed stack conservatively marks both results mixed because their individual
origins can no longer be distinguished.

When the window closes, the cast succeeds only if a town-returnable or mixed
stack remains in the caster's army. Moving borrowed troops down and then
returning all of them therefore consumes neither mana, movement, nor a daily
use.

## Town-name ABI correction

The legacy `_Town_` header declares `char name[12]` beginning at offset
`0xC8`. The executable actually stores a 16-byte MSVC `std::string` at
`0xC4`; offset `0xC8` is its internal `text` pointer. Treating that pointer's
bytes as a C string caused the corrupt glyphs in the old title.

The town chooser and system-window title now read the native string as:

```text
0xC4 allocator
0xC8 text pointer
0xCC length
0xD0 capacity
```

The localized title is copied into stock text widget ID 203 through
`textWidget::SetText` (`VA 0x57CA50`).

## Reversing and compatibility record

| Purpose | VA | Stable validation seam |
|---|---:|---|
| `TGarrisonWindow` constructor | `0x5D1200` | Full entry signature |
| `TGarrisonWindow` destructor | `0x5D14C0` | Full entry signature |
| `townManager::DoCommand` | `0x5D5010` | Body at `0x5D5016` plus Patcher chain type |
| Reset army-strip selection | `0x5D5930` | Full entry signature |
| Find title widget | `0x5FF5B0` | Full entry signature |
| Copy title text | `0x57CA50` | Full entry signature |
| Run modal window | `0x5FFA20` | Body at `0x5FFA26` (entry may already be hooked) |

The constructor ABI is:

```text
TGarrisonWindow* __thiscall (
    TGarrisonWindow* this,
    H3Hero* lowerHero,
    int upperOwner,
    H3Army* upperArmy)
```

The object is `0x78` bytes and is constructed in caller-owned stack storage,
matching `H3Hero::VisitGarrison` at `0x5D1530`. `0x5D14C0` is the non-deleting
destructor; it must not free that storage.

Current HD.WoG already owns a six-byte Patcher_x86 HiHook at `0x5D5010`.
New Spells validates that the existing entry is either original code or an
applied HiHook, then appends an extended THISCALL hook and always calls its
`GetDefaultFunc()` for permitted commands. This preserves the pre-existing
HD/WoG stack-experience and dialog behavior. Unsupported raw patches or ABI
signature changes disable only Reinforcements.

## Runtime verification still required

- Cast at every mastery and confirm nearest/any-town selection.
- Exercise exchange, merge, and split for all four provenance tags.
- Confirm the last stack of an occupying garrison hero cannot be removed.
- Confirm the title contains the real localized town name.
- Test WoG stack experience enabled and disabled.
- Save/reload mid-day and confirm the daily counter remains enforced; advance
  one day and confirm it resets.
- Confirm a no-op or fully returned borrow spends nothing, while a transfer
  left with the caster spends mana and movement exactly once.
