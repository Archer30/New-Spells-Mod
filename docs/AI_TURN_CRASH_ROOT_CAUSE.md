# AI-turn crash root cause

## Finding

The strongest AI-turn crash cause is an ABI mismatch introduced while moving
the SoD plugin from its older MSVC toolchain to a modern compiler.

`army` is not an ordinary plugin-owned class. heroes3.exe constructs, copies,
and destroys it using the executable's exact binary layout. The original layout
contains four old-MSVC vector objects, each 16 bytes wide. The previous ERA
adaptation compiled those fields as modern 12-byte `std::vector` objects.

This moved every later member and produced these incompatible layouts:

| Item | Executable ABI | Previous ERA build |
| --- | ---: | ---: |
| `sizeof(army)` | `0x548` | `0x538` |
| `army::AI_target` | `0x538` | `0x528` |
| Difference | — | 16 bytes short |

The AI enchantment evaluators allocate temporary stacks and call the engine's
`army` copy constructor. heroes3.exe then writes its full 0x548-byte object into
a 0x538-byte C++ allocation. The destructor also reads vector state and later
members at the executable offsets, not the offsets used by the plugin. The
result is heap corruption concentrated in AI spell evaluation, matching the
reported crashes during AI turns.

## Repair

`HoMM3API.h` now uses a trivial `exe_vector<T>` binary view for those fields.
`NewSpells.h` similarly uses an explicit 32-bit-word `exe_bitset<N>` for
executable-owned bitsets. Neither wrapper owns memory or runs a compiler-runtime
destructor; engine methods retain ownership.

Compile-time assertions verify:

- x86 compilation and 32-bit pointers
- `sizeof(exe_vector<T>) == 0x10`
- critical `army` member offsets
- `offsetof(army, AI_target) == 0x538`
- `sizeof(army) == 0x548`
- AI helper object sizes and offsets
- combination-artifact record size and bitset placement

The `army` wrapper invokes the engine destructor once, after which its trivial
views perform no second destruction.

## Related hardening

AI spell paths now validate stack coordinates, combat manager state, caster
side, spell mastery, duration, health, and target pointers before indexing
sidecar arrays or calling engine methods. Temporary-stack health and summoning
calculations use bounded 64-bit or floating-point intermediates and saturate
before returning 32-bit engine values.

This diagnosis is source- and ABI-proven. Runtime stress testing remains needed
to validate the complete mod stack and every third-party hook combination.
