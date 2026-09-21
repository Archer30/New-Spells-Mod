# New Spells Expansion

New Spells Expansion supplies additional spells to the **New Spells** ERA mod.
Version 1.1.0 contains **Reinforcements** (96) and **Blizzard** (97). New Spells
must be installed and enabled, and this expansion must load after it.

Its spell records use the provider-unique
`Lang/NewSpellsExpansion.NewSpells.json` filename so loading the expansion
cannot replace New Spells' 18-entry core JSON catalog.

Reinforcements is a level-3 Fire spell. At no or Basic Fire Magic it selects
the closest eligible town; at Advanced or Expert it lets the player choose any
eligible town. It opens Heroes III's system garrison window and allows the hero
to borrow troops. Borrowed troops may be returned unless they have been merged
with an army stack that originally belonged to the hero.

The spell allows 1, 2, 3, or 4 successful casts per day according to mastery.
A successful cast costs 300, 200, 200, or 100 movement points respectively.
None through Advanced require at least 300 movement before casting; Expert
requires 200. Mana and sound are committed by New Spells only after troops were
transferred. Closing the chooser or transferring no troops consumes nothing.

Do not enable the legacy **Reinforcements Spell** mod at the same time. This
expansion keeps the `NewSpells.Reinforcements` version-1 save section, so daily
cast counts written by the formerly integrated implementation remain valid.

Reinforcements Spell was created by daemon_n from an idea by ShimmY and Master
of puppets. Chinese localization: MoonHeart. The imported work remains under
the included MIT license. External-provider extraction and ERA maintenance:
Archer30.

Blizzard is a level-5 Water spell. Mana costs are 25/20/20/20, and damage is
20 × Spell Power + 30/30/60/120. It hits all stacks within two hexes of the
selected hex, including the center and friendly troops. Immunity, resistance,
and native spell-damage modifiers apply.

Survivors lose 2/2/4/4 base speed, with effective speed never below 1. The
penalty lasts for the battle, survives death and resurrection, and cannot be
removed by Cure, Dispel, Haste or removing ordinary Slow. Recasting deals damage
again but keeps the first Blizzard penalty. Combat AI values both damage and
the slowing of survivors, including friendly casualties. Quick Combat excludes
external spells, following the framework's existing policy.

Blizzard was created by daemon_n with ShimmY; port and AI corrections by
Archer30. Disable the legacy **Blizzard Mod** when using this expansion.
Remove Obstacle remains spell 64, with its original animation. This release
requires the accompanying New Spells 2.12.2 core integration update.
