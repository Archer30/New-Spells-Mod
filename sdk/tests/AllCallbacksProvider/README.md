# All-callbacks routing provider

This DLL is a non-shipped integration probe. One `RegisterProviderBatch` call
registers four fixed IDs that exercise every combat target-dispatch mode and,
collectively, every ABI-v1 callback:

| ID | Spell key | Kind | Target mode | Purpose | Capabilities |
|---:|---|---|---|---|---:|
| 123 | `TargetedTimedStatus` | combat | targeted | Timed apply/round/remove, cure/dispel, creature and ERM sources | 2046 |
| 124 | `AreaDamage` | combat | area | Area damage/cast routing | 1926 |
| 125 | `GlobalHybrid` | hybrid | global | Adventure and combat casting plus both AI evaluators | 3207 |
| 126 | `Summon` | combat | summon | Neutral summon dispatch; provider-owned summon state | 1926 |

The callbacks validate the matching context ABI, increment counters, and
return deterministic values. Combat AI returns score `1000 + spellId`, target
hex `spellId - 123`, and `castNow = 1`; adventure AI does the same for ID 125.
The probe deliberately performs no gameplay mutation.

The exported `NewSpellsSdkTestGetCallbackCount` indexes are:

0. adventure validation
1. adventure cast
2. combat-target validation
3. combat cast
4. status apply
5. status round
6. status remove
7. cure/dispel
8. battle lifecycle
9. creature cast
10. ERM cast
11. combat AI
12. adventure AI

`NewSpellsSdkTestGetCallbackCount(counter)` reads the aggregate count across all
four IDs. `NewSpellsSdkTestGetSpellCallbackCount(spellId, counter)` reads one
ID's count. `NewSpellsSdkTestResetCallbackCounts()` clears both counter sets.

The JSON records deliberately use stable numeric animation index 0, exact
capability masks, no native adventure-AI flag, and explicit target modes that
match their native targeting flags. The provider returns `COMMITTED` without a
gameplay effect solely so a test installation can verify dispatch and resource
epilogues. Never distribute it in a release mod.
