# Local mod profiles

Open the mods directory from the launcher. Each direct subdirectory contains a
`mod.json` manifest:

```json
{
  "schema": 1,
  "id": "example.camera",
  "name": "Example camera mod",
  "version": "0.1.0",
  "description": "An example metadata-only entry."
}
```

The ID contains letters, digits, periods, underscores or hyphens. IDs must be
unique across installed manifests. The launcher displays malformed or duplicate
entries and prevents enabling them. Discovery only reads metadata; it does not
execute mod code. Enabled IDs are persisted separately in each named profile.

Deployment into an isolated game copy is not implemented yet. An enabled profile
entry is a saved selection, not proof that a mod has been loaded into the game.

Community-required profiles and code approval are specified in the planned
[mod trust model](mod-trust.md). Schema 1 above is metadata only and does not
grant execution permission or describe a deployable package.
