# Amethyst after-attack dispatcher compatibility

## Runtime evidence

- Target module: `h3era HD.exe`, PE32, preferred/runtime base `0x00400000` in
  the 2026-08-25 test process.
- Routine: battle stack after-attack ability dispatcher.
- New Spells hook: `0x00440903`, six-byte `LoHook`, owner
  `HD.Plugin.H3.NewSpells`.
- Stock instructions captured by the New Spells trampoline:

  ```asm
  00440903  mov eax,[esi+34h]
  00440906  add eax,-63
  00440909  cmp eax,72
  0044090C  ja  004412AB
  ```

- Amethyst initializes after New Spells in the active mod order and performs
  direct, untracked writes from `CreateAdditionalTables()`:
  - `0x00440908 = 0`, changing the in-place instruction to `add eax,0`;
  - `0x00440909..0x00440911 = NOP`, removing the stock range check;
  - the table operand at `0x00440916` is redirected to Amethyst's extended
    raw-creature-ID table.

The already-created New Spells trampoline retains `add eax,-63`, while the
later Amethyst dispatcher expects a raw creature ID. For IDs below 63 this
causes an out-of-bounds table read; selector zero is Amethyst's
`ATT_VAMPIRE`, producing apparent Drain Life on ordinary creatures.

## Compatibility path

`DrainLife()` validates the complete live Amethyst-style dispatcher contract:

- byte `0x00` at `0x00440908`;
- nine NOPs at `0x00440909..0x00440911`;
- intact `xor ecx; mov cl,[eax+table]` opcodes;
- a non-null, non-stock table pointer at `0x00440916`.

For a stack without the New Spells Drain Life enchantment, the hook then sets
`EAX` to the raw creature ID and resumes at `0x00440909`, bypassing only its
stale relocated subtraction. If the contract is absent, the stock relocated
instructions execute unchanged. An enchanted stack still redirects directly
to the native drain handler at `0x00440921`.

## Validation

- `Release|Win32` rebuilt with Visual Studio 2022 MSBuild and `v141_xp`.
- Static runtime: `/MT`.
- Output: PE32 x86, OS/subsystem 5.01.
- Built, packaged, and deployed DLLs compared byte-for-byte.

Manual testing remains required with a fully restarted game:

1. Low-ID ordinary creatures must not drain life.
2. Vampire Lords must retain native life drain.
3. A creature enchanted with New Spells Drain Life must drain life.
4. Amethyst custom creatures with configured attack effects must dispatch the
   correct effect.
5. Repeat melee, retaliation, ranged, human, and AI-controlled battles.

TUM's custom spell IDs 83-86 still overlap New Spells IDs 83-86. That is a
separate data-ownership issue and is not addressed by this dispatcher fix.
