# Current-client integration contract

Game integration is not implemented. The launcher includes an exact-file identity
inspector to establish reproducible starting points for adapter development.

## Inspect an installed client

Use **Settings > Inspect client files**, or the packaged console tool:

```console
CommunityDiversInspect.exe --game "C:/Games/Helldivers 2" --output client-inspection.json
```

The tool reads the Steam build ID and SHA-256/byte size of:

- `bin/helldivers2.exe`
- `data/game/game.dll`
- `bin/PartyWin.dll`
- `bin/PlayFabMultiplayerWin.dll`

Hashes are calculated with Windows BCrypt. Read-only handles withhold write/delete
sharing across the inspected file set for the whole pass. Steam build metadata is
checked again before accepting the result. Missing
or unreadable files are reported individually. Reports contain relative binary
paths and build metadata, without account identifiers or an installation path.

The Settings action runs asynchronously and saves `client-inspection.json` in the
launcher's data directory. Changing the selected installation while an inspection
is pending discards that result.

## Observed build catalog

`compatibility/client-builds.json` records observed identities. A known identity
requires the Steam build ID and all four byte sizes and SHA-256 values to match.
A changed binary or partial inspection does not match. These four fingerprints
identify the inspected client modules; they do not verify every installed asset.

The initial record is Steam build `26088801`, observed on 26 October 2026. Its
integration state is `not_implemented`. A catalog match is a diagnostic result,
not a supported adapter, proof of game ownership or permission to launch.
Changing catalog flags cannot enable the missing adapter in the launcher.

## Adapter acceptance gates

A future adapter needs separate, build-specific evidence for all of these gates:

1. Startup and native integration on an isolated copy of the current owned game.
2. Community bootstrap/account/mission calls with versioned request and callback
   contracts, backed by a reachable community service.
3. Verified separation of local settings, player progression and Steam Cloud data.
4. Verified prevention of official gameplay/progression connections by the modified
   client, including fallback and failure paths.
5. A solo ship-to-mission loop against the community backend, followed by
   compatible-client multiplayer and relay testing.

The launcher currently returns `integration.available: false` for every inspection.
A server's advertised build list and a local fingerprint match cannot override
that implementation state. Old-build function addresses and loader support are
insufficient evidence for any current-client gate.
