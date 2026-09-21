# Repository organization

The repository contains three maintained plugin projects: `NewSpells/`,
`NewSpellsEditor/`, and `NewSpellsExpansion/`. Their sources stay together so
spell definitions, provider contracts, and editor/map support can evolve together.

- `assets/` and `NewSpellsExpansion/assets/`: build inputs and donor provenance.
- `sdk/`: external-provider API, examples, and contract checks.
- `tests/`: regression tests and opt-in runtime probes.
- `tools/`: build, asset, and deployment helpers.
- `docs/`: contracts, investigations, validation records, and publication notes.
- `dist/New Spells/` and `dist/New Spells Expansion/`: current package resources
  and metadata. Locally packaged plugin binaries remain Git-ignored.
- `build/`: recommended disposable output directory for new builds; ignored.

## Archived local material

On 2026-09-21, old build output, nine obsolete release ZIPs, the copied game
sandbox, scratch investigations, downloaded reference material, machine-specific
project settings, the superseded `.vcproj`, and its AppWizard readme were moved to:

`D:\Repos\_archives\New Spells Mod\20260921-cleanup`

The archive's `manifest.json` records original relative paths, file counts,
and sizes. Modified documentation/project files were backed up there too.
Nothing in that archive is needed to compile the tracked source. Historical
reports that mention `work/`, `scratch/`, or `reference/` refer to archived evidence.

Runtime probes need an explicitly prepared disposable game copy. Restore or
prepare one under `work/` and provide the probe parameters as appropriate;
it is intentionally absent from source control. The installed-startup probe
accepts `-ReleaseMapPath` and defaults to `build/core/obj/NewSpells.map`.

## Deliberately retained duplicates

- `sdk/include/NewSpellsProviderApi.h` is the distributable SDK copy of the core
  header; `sdk/tests/verify_header_sync.ps1` checks synchronization.
- Reinforcements' MIT notice belongs both with donor provenance and in the
  installable expansion package.
- `dist/New Spells Expansion/BLIZZARD_PORT.md` accompanies the expansion package;
  `docs/BLIZZARD_PORT.md` is the source-tree copy.
- Donor archives and extracted source graphics remain because asset validation
  and provenance use them. They are not old installable release packages.
