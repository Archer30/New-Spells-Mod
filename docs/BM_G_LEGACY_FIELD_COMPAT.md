# BM:G compatibility in New Spells 2.12.3

Implemented and validated on 2026-09-20 against the supplied `D:\Games\Heroes 3 ERA\Debug\New spells x acm erm bug` capture and the installed ERA 3.9.31 receiver.

## Compatibility policy

Loading New Spells must not introduce an ERM error for a `BM:G` command accepted by the same ERA installation without its runtime changes. The deliberate exception is IDs **81–126**: these always retain New Spells semantics, including when a provider is disabled or absent. This supersedes the earlier proposal to support only 212/213 and migrate other callers to `UN:C`.

| Index | Interpretation |
|---|---|
| 0–80 | Native spell duration and relocated mastery; original operand value domain, with New Spells' existing removal behavior |
| 81–126 | Existing New Spells/provider validation and spell behavior |
| Every other signed 32-bit index | Original field-address calculations for both operands, translating original mastery cells to relocated storage |

No whitelist, same-stack boundary, dummy third-operand result, or script migration is introduced. This preserves permissive memory access; it does not make previously invalid accesses safe. ERA's Apply routine can read an address even for `/d`, so unused operands are not a memory-safety guarantee.

## Confirmed failure

The supplied log reports `Unknown error` in `advanced classes skills.erm:4597–4598`:

```erm
!!BMy72:G213/?y2/?t I?y90;
!!BMy82:G213/?y3/?t I?y91;
```

These are attacker/defender luck queries in ACM's BG0 attack handler, followed by luck resets. The previous New Spells hook rejected index 213 because it was not a valid spell ID. The same guard rejected other established field aliases. Simply widening the spell-ID check would still index the wrong storage and impose spell-specific value restrictions and cancellation on unrelated fields.

## Observed scope

The read-only inventory is in `scratch/bmg-audit/inventory.json`; its script and text output are alongside it. It scans complete BM receiver commands, including multiple G subcommands and commands spanning lines, rather than only `:G` text. Disabled `*!` commands are excluded. This is static candidate-call inventory, not proof every trigger/option executes.

| Scope | Files scanned | Observed non-spell literal indices |
|---|---:|---|
| Supplied loaded-script snapshot | 348 | -81, 212, 213 |
| All installed mod script files, including inactive mods and backup directories | 1,000 | -95, -81, 212, 213 |
| Local `D:\Repos` ERM corpus, including historical/development copies | 1,322 | Many negative and positive field indices; listed below |

Named `BMG_FIELD_MORALE`/`BMG_FIELD_LUCK` references were also checked. The supplied snapshot has resolved these to 212/213. Its 32 variable-index call sites were reviewed: they use spell loops, spell lists, or passed spell IDs. One unfinished TUM Elemental Marks function loops 97..100 but exits unconditionally first; that is neither an active field alias nor a reason to remap provider spell IDs. This is not an exhaustive proof over arbitrary map scripts, generated ERM, or future callers.

The loaded snapshot contains 33 non-spell G subcommands:

| Script | G212 | G213 | G-81 |
|---|---:|---:|---:|
| advanced classes skills.erm | 9 | 11 | 0 |
| 30 wog - enhanced secondary skills.erm | 3 | 3 | 0 |
| 100 tum - reborn.erm | 2 | 0 | 0 |
| option 997 - grand manouvre.erm | 3 | 0 | 0 |
| option 795 - night scouting.erm | 0 | 1 | 1 |

Important additional sites:

| Index | Actual primary field | Example |
|---:|---|---|
| -81 | `army + 0x54` / 84: battle-resurrected casualties, lost after battle | Installed ERA Scripts Eng, `option 795 - night scouting.erm:885` |
| -95 | `army + 0x1C` / 28: target hex used for an attack | Installed Second Henchmen Reborn, `900 sechen - main.erm:1048,1053` |
| 212 | `army + 0x4E8` / 1256: current calculated morale | ACM, WoG Scripts, TUM, ERA Scripts, Mixed Neutrals |
| 213 | `army + 0x4EC` / 1260: current calculated luck | ACM, WoG Scripts, ERA Scripts, Knightmare Kingdoms, BattleHeroes |

The broader repository literal set is:

```text
-100, -95, -93, -92, -91, -87, -86, -81,
-75, -74, -73, -70, -68, -67, -66, -13, -12,
195, 200, 206, 207, 209, 212, 213
```

For example, `D:\Repos\fengmo-2-development\FM\Data\s\era - EasyDo.erm` reads poison, shield, disease penalties, and defense bonuses using 195/200/206/207/209. Its function 100630 additionally feeds `Gy1` from this literal lookup table:

```text
176, 177, 178, 179, 180, 181, 182, 184, 185, 186,
187, 188, 189, 191, 192, 193, 194, 196, 197, 198,
199, 200, 201, 203, 204, 205, 211
```

The active ERA framework also publishes eleven `BMG_FIELD_*` index constants in `lib\9999 era - consts.erm:1316`: -100, -90, -89, -88, -87, -86, -83, -82, -81, 212, 213. Consequently, these field tricks extend beyond ACM and are established scripting conventions. These uses are supported by the 2.12.3 compatibility path; no script migrations are required.

## Implementation and runtime contract

The original executable receiver selects BM stack 0–41 and validates syntax before the hook. It calculates, with wrapping x86 32-bit arithmetic:

```text
primary address   = stack + 0x198 + 4 * index
secondary address = stack + 0x2DC + 4 * index
```

`NewSpells/ErmBattleFields.h` keeps this calculation unsigned to avoid C++ signed-overflow behavior. `resolveLegacyErmBattleField` checks the resulting address against all 42 physical stack slots, each 0x548 bytes. Addresses at original mastery slots (offsets 0x2DC through 0x41C, inclusive) map to `activeSpellMastery[physicalSlot / 21][physicalSlot % 21][spell]`. Mutable stack side/index fields do not select this storage. All other addresses are passed unchanged to ERA Apply.

This translates primary aliases 127–161 to original mastery[46–80], plus any alias into another stack's mastery. For example, primary index 465 targets the next physical stack's mastery[46]; secondary index 384 targets the same cell. Both operands are independently resolved. Addresses before/after the selected stack are not rejected.

| Index | Primary offset | Secondary offset |
|---:|---:|---:|
| -95 | 0x1C (attack target hex) | 0x160 |
| -81 | 0x54 (battle-resurrected casualties) | 0x198 (duration[0]) |
| 127 | 0x394 (relocated mastery[46]) | 0x4D8 |
| 212 | 0x4E8 (morale) | 0x62C |
| 213 | 0x4EC (luck) | 0x630 |

The legacy path calls the installed ERA Apply at 0x74195D twice, in native order, with size 4 and operand numbers 1 and 2. It performs no speculative field read, rollback, new error check, or spell cancellation. Raw integers, float bit patterns, arithmetic, comparisons, `/d`, and third-operand queries/writes retain Apply's behavior. In particular, a failed second operand does not undo a successful first assignment.

Native IDs 0–80 use the original duration cell and relocated mastery. Negative duration and mastery outside 0–3 are no longer rejected. Apply errors retain native reporting and partial writes. Successful nonzero-to-zero duration transitions still call New Spells' spell removal routine with the previous duration/mastery restored for cancellation.

IDs 81–126 keep the previous registration, capability, real-stack, value, callback, and removal checks. A disabled, unsupported, or unregistered provider never becomes a legacy alias.

Existing installation signatures and Apply contract validation remain in force. Both current game executables are x86 PE32 with image base 0x400000; hook VA 0x75F334 is RVA 0x35F334 / file offset 0x2FA334. The replaced five bytes are `push 1; mov ecx,[ebp+0x14]`. Receiver ABI: index at EBP-0x24, selected stack at EBP-0x10, command at EBP+0x14. Normal continuation is 0x75F370; the existing added-spell error exit is 0x75F867.

The supplied patch dump identifies a single 5-byte LoHook owned by `HD.Plugin.H3.NewSpells`, and ERA's raw Apply patch at 0x74195D. Byte evidence is in `scratch/bmg-audit/binary-verification.json` and `bmg-disassembly.txt`. The later bounds checks in local `D:\Repos\wog\T1\Monsters.cpp` are absent from this installed executable and were not substituted for its behavior.

## Validation results

- **63,671 address checks passed**: every byte of all 42 stack slots, original mastery boundaries, aliases 127–161, another stack's mastery, and extreme/wrapping 32-bit indices. Invalid addresses are checked as integers without dereferencing them.
- **2,454 control-run checks passed** with all New Spells runtime initialization bypassed in a test-only DLL. Only the harness's startup/error interception hooks are installed. This records original receiver results with original physical mastery storage.
- **7,377 enabled-run checks passed** with New Spells loaded. The real ERA parser/Apply executes the same 1,226 command sequences. Each sequence compares the standalone control, the original BM receiver with only its New Spells hook undone, and the fixed receiver. Original mastery cells are deliberately poisoned during the fixed run to verify relocation.
- Coverage includes stacks 0, 20, 21, 41; every index -102 through 235 except reserved IDs 81–126; negative animation/stack fields; modifiers 195/200/206/207/209; morale/luck; reads, assignments, arithmetic, comparisons, negative values, float bits, `/d`, and third operands. Controlled cross-stack accesses include addresses before the selected stack and both operands targeting another stack's mastery. Edited side/index fields do not break physical-slot translation.
- Native negative duration/mastery, malformed argument counts/index operands/variables, invalid BM stack numbers, and partial writes on operand failure match the original receiver. Normal Haste and added Fear application/removal passed using the game routines. Added-spell value rejection, a provider callback, disabled-provider rejection, missing capability, and absent reserved ID 126 were checked.
- ACM attacker/defender luck query/reset, Night Scouting casualty accounting, and Grand Manouvre morale save/clear/restore sequences passed through ERA on controlled stacks. **Full interactive battle replay was not completed.** These checks do not simulate the entire scripts' triggers, combat, or saved-game state.

The probe runs only in a disposable workspace game copy. It submits each command without the script-only `!!` prefix required to be omitted by `ExecErmCmd`. Its error hooks count native errors without showing dialogs; its synthetic stack storage is never used for gameplay. Test builds terminate after the probe and must not be deployed. No test macro is enabled in the release DLL.

To reproduce (the existing disposable copy has Expansion active for the provider fixture):

```powershell
python tools/build_release.py tests/ErmBattleFieldsTests.vcxproj work/bmg-address-tests
& work/bmg-address-tests/ErmBattleFieldsTests.exe
python tools/build_release.py NewSpells/NewSpells.vcxproj work/bmg-baseline-build --bmg-baseline-probe
python tools/build_release.py NewSpells/NewSpells.vcxproj work/bmg-native-build --bmg-native-probe
& tests/ProbeErmBattleFields.ps1 -ProbeBuild work/bmg-baseline-build
Copy-Item work/bmg-native-probe-log.txt work/bmg-baseline-probe-log.txt
& tests/ProbeErmBattleFields.ps1
```

## Release and limits

The production core was built in `work/bmg-release-2.12.3` with
`Release|Win32`, `v141_xp`, static `/MT`, and OS/subsystem target 5.01.
The PE audit confirms x86 PE32 and only XP-compatible KERNEL32 static imports;
there is no added runtime dependency. ERA and Patcher remain dynamically
resolved dependencies of the established installation. No XP VM proof is
claimed. The matching `.dbgmap` was generated automatically and its handler
and resolver RVAs were checked against this build's linker map. Test-probe
symbols and diagnostic markers are absent from the production DLL.

The following four files were packaged in `dist/New Spells` and deployed,
byte-for-byte verified, to `D:\Games\Heroes 3 ERA\Mods\New Spells`:

- `EraPlugins/NewSpells.dll` (193,024 bytes)
- `DebugMaps/NewSpells.dbgmap` (101,147 bytes)
- `README.md`
- `mod.json` (version 2.12.3)

Source originals are in `backups/20260920-011724-before-bmg-compat`; installed
originals are in `backups/20260920-013909-installed-before-2.12.3`. Follow-up
source/probe revisions were also backed up under `work/bmg-*before*`.
The deployment record is `work/bmg-deployment.json`.

**Installed startup passed** using `tests/ProbeNewSpellsStartup.ps1`: the live
handler matches the deployed DLL after PE relocation, the BM:G hook is present,
the core spell count is 96, and all 18 built-in records are valid. ERA reports
the expected virtual `EraPlugins/NewSpells.dll` path, so the audit checks mapped
handler bytes against the physical mod DLL rather than assuming those paths
are identical. `Mods/list.txt`, the installed editor DLL, and the expansion
DLL/debug map were verified unchanged after startup. Evidence is in
`work/bmg-installed-startup.txt` and `work/bmg-release-pe-audit.txt`.

Expansion/editor binaries, other mods' scripts, and mod ordering are outside this change. A full battle replay and a clean Windows XP SP3 VM run remain manual validation. The compatibility exception for indices 81–126 and the original risk of invalid raw memory access are intentional.
