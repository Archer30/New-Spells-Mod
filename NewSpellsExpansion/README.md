# New Spells Expansion plugin source

This project supplies external spells to **New Spells** through
`NewSpellsProviderApi.h`. Reinforcements is registered at the stable spell ID
96 and Blizzard at 97. Initialization is independent: Blizzard compatibility
failure does not disable Reinforcements. New Spells owns the common spell
transaction, including mana and sound; this plugin owns spell-specific effects.

Blizzard preserves the donor's damage, radius and costs. Native area execution
shares one resistance roll between damage and the permanent speed penalty.
Recasting retains the first applied penalty. Its AI evaluates every legal hex,
including slowing value, surviving units and friendly collateral. Remove
Obstacle remains at 64. See `../docs/BLIZZARD_PORT.md` for details and validation.

The project is deliberately Win32-only and uses the `v143` toolset with the
static C runtime. Build `Release|Win32` for the distributable ERA plugin.
The shared build target emits a matching `DebugMaps/NewSpellsExpansion.dbgmap`.
Run `build_assets.ps1` to rebuild the icon ZIP and expansion-owned animation PAC.
