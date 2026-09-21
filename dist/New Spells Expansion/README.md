# New Spells Expansion

Version **1.1.0** adds two spells to the New Spells ERA mod: **Reinforcements** and **Blizzard**. They have their own spell IDs and do not replace existing spells.

## Installation

1. Install and enable **New Spells**. Use the accompanying **2.12.3** release.
2. Extract this expansion into your game's `Mods` folder, enable it, and load it after New Spells.
3. Disable the older **Reinforcements Spell** and **Blizzard Mod** packages when using this expansion.

With the expansion enabled, both spells also appear in the New Spells map-editor checklist. Restart the editor after changing enabled mods.

## Reinforcements

A level-3 Fire adventure spell (ID 96) that lets your hero borrow troops from an eligible town through the normal garrison window.

Without Fire Magic or at Basic mastery, it selects the closest eligible town. At Advanced or Expert, you can choose an eligible town. Borrowed troops can be returned unless they have been merged with a stack that originally belonged to the hero.

| Fire Magic | None | Basic | Advanced | Expert |
|---|---:|---:|---:|---:|
| Successful casts per day | 1 | 2 | 3 | 4 |
| Movement spent per successful cast | 300 | 200 | 200 | 100 |
| Movement required before casting | 300 | 300 | 300 | 200 |

Closing the selection window or transferring no troops spends no mana or movement and does not use a daily cast. Daily cast counts from the earlier integrated Reinforcements implementation remain compatible.

Reinforcements is available to human players; adventure AI does not cast it.

## Blizzard

A level-5 Water combat spell (ID 97) that damages all stacks within two hexes of the selected hex, including the center and **friendly troops**. Normal immunity, resistance, and spell-damage modifiers apply.

| Water Magic | None | Basic | Advanced | Expert |
|---|---:|---:|---:|---:|
| Mana cost | 25 | 20 | 20 | 20 |
| Damage | 20 × Spell Power + 30 | 20 × Spell Power + 30 | 20 × Spell Power + 60 | 20 × Spell Power + 120 |
| Base speed reduction | 2 | 2 | 4 | 4 |

Survivors retain the speed reduction for the rest of the battle, even after death and resurrection. Effective speed cannot fall below 1. Cure, Dispel, and Haste do not remove the Blizzard penalty. Casting Blizzard again deals damage again but does not stack or replace the first speed penalty.

Combat AI can cast Blizzard and considers damage, slowing, and harm to friendly troops. External spells are excluded from Quick Combat.

## Credits

- **Reinforcements:** daemon_n; idea by ShimmY and Master of puppets.
- **Chinese localization:** MoonHeart.
- **Blizzard:** daemon_n with ShimmY.
- **ERA integration, maintenance, and Blizzard AI corrections:** Archer30.

The imported Reinforcements work retains the included MIT license.
