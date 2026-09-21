# Minimal adventure provider

This is a buildable, non-shipped ID97 provider skeleton. Copy the DLL project,
choose an unused stable ID, change both identity strings, and make the JSON
identity match exactly.

`ValidateAdventure` shows the required context checks. `CastAdventure`
intentionally returns `UNSUPPORTED`: replace the marked placeholder with the
actual provider-owned effect and return `COMMITTED` only after it persists.
New Spells will then own the single mana/sound transaction.
