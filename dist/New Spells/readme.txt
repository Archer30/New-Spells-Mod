New Spells Plugin

Quite recently (August) I was challenged with a task of adding new spells to the game via a plugin for the HD mod. I said then that it's quite easy to implement, meaning the very core mechanics.

Now I and Rolex, another user from HandBookHMM (who is the graphic designer of the project btw), can present the plugin which actually allows you to add new spells, with some spells already added by us as examples.

16 new spells have been added (well, some of them are not quite new, but we don't have a lot of original graphics and animations anyway).

Earth Magic: Poison, Disease, Fear, Death Cloud, Drain Life
Fire Magic: Mobility, Age, Death Blow, Behemoth's Claws, Incineration, Explosion, Summon Firebird
Water Magic: Toughness
Air Magic: Eye of the Magi, Summon Sprite
And Summon Magic Elemental which went to every magic school.

All spell work and AI can use them, except Mobility.

Added new special features for Explosion and Incineration spells which add a lot to the tactical component of the battle and can turn the tide of battle in your favor.

Added two new spells - Hour of Power and Golden Touch - by Szaman.

Don't miss Golden Touch if you think money is the key to success.

Thanks to daemon_n for the valuable bug report on Admiral's Hat 

ERA 2.12.1 integration: New Spells contains 18 built-in spells and publishes a
versioned native provider framework for separately packaged spell IDs 96-126.
Install New Spells Expansion after this mod to add Reinforcements at ID 96
without replacing Disguise. The legacy Reinforcements Spell mod is
incompatible with the expansion, not with this core framework.

External providers use unique Lang/*NewSpells.json filenames. This prevents a
later provider from replacing the core spell catalog through ERA's VFS.

The 18 built-ins and valid declarations from active external provider mods
appear in the original map editor Spells tab. They can be disabled per
standalone H3M map; OK saves the choices and Cancel discards them like the
vanilla spell checkboxes.

ERA 2.11.1 integration: NewSpells.dll publishes a versioned, read-only battle-
AI spell query and a dedicated owner for the existing Fear melee guard. This
lets cooperating general AI plugins use live New Spells mana and availability
data without duplicating the same combat hook.
