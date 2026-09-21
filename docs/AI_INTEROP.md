# New Spells AI interoperability contract

Version 2.11.1 publishes a read-only, versioned Win32 C ABI for battle-AI
plugins that need New Spells' live hero spell state. Policy remains in the
consumer: this interface does not classify spells, assign threat values, or
change AI decisions.

## Discovery and ABI

Find the Patcher_x86 variable
`HD.Plugin.H3.NewSpells.AI.SpellQuery.v1`. Its 32-bit value is a pointer to the
following packed-4 structure, declared in `NewSpells/NewSpellsAiInterop.h`:

```cpp
#pragma pack(push, 4)
struct AiSpellStateV1 {
    uint32_t size;
    int32_t known;
    int32_t enabled;
    int32_t mastery;
    int32_t effectiveManaCost;
    int32_t currentMana;
};

struct NewSpellsAiInteropV1 {
    uint32_t size;
    uint32_t abiVersion;
    uint32_t capabilities;
    int32_t (__stdcall *QueryHeroSpell)(const void* combatManager,
        int32_t casterSide, int32_t spellId, AiSpellStateV1* outState);
};
#pragma pack(pop)
```

Version 1 has `abiVersion == 1`, structure sizes 16 and 24 bytes on Win32, and
capability bit 0 (`NEWSPELLS_AI_CAPABILITY_QUERY_HERO_SPELL`). A consumer must
validate the provider size, ABI version, capability, and callback before use.
It must also tolerate the variable being absent when New Spells is not loaded.

Before each call, zero `AiSpellStateV1` and set its `size` to 24. The callback
returns 1 only when the pointer is the current live combat manager, the caster
side is attacker (0) or defender (1), the side has a valid hero, and the spell
is a structurally defined active hero spell. It returns 0 for invalid input.
When the supplied output structure is large enough, a failed query leaves its
fields in a closed state (`known == 0`, `enabled == 0`, `mastery == -1`).

Successful fields are live values:

- `known`: the caster hero's extended spellbook byte is nonzero.
- `enabled`: the game's current spell-disable array permits the spell.
- `mastery`: the engine result for the battle's current magic terrain (0..3).
- `effectiveManaCost`: the engine result using the opposing live army group,
  including creature-based cost modifiers.
- `currentMana`: the caster hero's current spell points.

The provider is published only after New Spells finishes initialization. If
the variable name already exists, New Spells logs the collision, does not
overwrite the existing value, and leaves its own interop unpublished.

## Fear hook ownership

The existing Fear melee guard at `0x422088` is now registered only by the
Patcher owner `HD.Plugin.H3.NewSpells.FearMeleeGuard.v1`. Its callback behavior
is unchanged: it redirects a feared active stack past melee attack execution.
No register repair is added. If the dedicated owner name collides, New Spells
logs the collision and does not install this hook.

Other New Spells Fear hooks remain under `HD.Plugin.H3.NewSpells`; the
dedicated owner covers only `0x422088` so another AI plugin can detect and
avoid duplicating this exact guard.
