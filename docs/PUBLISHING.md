# GitHub publication preparation

The maintained checkout is `D:\Repos\New Spells Mod`. The current core package
version is 2.12.3; expansion/editor sources retain their existing versions.

## Prepared source snapshot

The initial Git commit includes source, build projects, tests, documentation,
SDK examples, translations, package resources, and source asset provenance.
It excludes local game copies, backups, investigation output, compiled DLLs,
debug maps, and historical release archives. Historical local files have been
moved outside the repository; see `REPOSITORY_LAYOUT.md`.

Build the core from a VS 2022 installation with v143 and SDK 10:

```powershell
python tools/build_release.py NewSpells/NewSpells.vcxproj build/core
python tools/build_release.py tests/ErmBattleFieldsTests.vcxproj build/bmg-tests
& build/bmg-tests/ErmBattleFieldsTests.exe
```

The helper currently uses the standard VS 2022 Community installation path;
other installations can invoke their MSBuild with Release/Win32 and explicit
OutDir/IntDir. Build the solution for editor and expansion binaries too.
Package each DLL in its appropriate EraPlugins/EraEditor directory and its
matching `.dbgmap` in DebugMaps. Do not include intermediate linker maps.
The complete locally prepared 2.12.3 core package is retained under dist.

## Before publishing

1. Resolve the core/upstream license coverage documented in
   `../THIRD_PARTY_NOTICES.md`; no blanket license has been assigned.
2. Select the GitHub owner, repository name, and public/private visibility.
3. Create an empty remote repository, add its exact URL as origin, and push
   the prepared main branch. No remote repository or upload was made during
   this preparation.
4. Attach installable packages to a GitHub Release rather than adding generated
   plugin binaries or historical release ZIPs to source history.

The BM:G command tests passed during development; full battle replay and a
clean XP SP3 VM run remain outstanding as documented in
`BM_G_LEGACY_FIELD_COMPAT.md`.
