# Current-client integration contract

Game integration is not implemented. The launcher includes an exact-file identity
inspector to establish reproducible starting points for adapter development.
The [current startup investigation](client-startup.md) records the observed
preloader/runtime process dependency and the limits of an offline contained probe.

## Inspect an installed client

Use **Settings > Inspect client files**, or the packaged console tool:

```console
CommunityDiversInspect.exe --game "C:/Games/Helldivers 2" --output client-inspection.json
```

The tool reads the build ID from Steam metadata or a completed preparation receipt,
and calculates the SHA-256/byte size of:

- `bin/helldivers2.exe`
- `data/game/game.dll`
- `bin/PartyWin.dll`
- `bin/PlayFabMultiplayerWin.dll`

Hashes are calculated with Windows BCrypt. Read-only handles withhold write/delete
sharing across the inspected file set for the whole pass. Steam build metadata is
checked again before accepting the result. Missing
or unreadable files are reported individually. Reports contain relative binary
paths and build metadata, without account identifiers or an installation path.

For an independent copy, **Inspect client files** reads its preparation receipt and
requires all four current binary fingerprints to match before using the recorded
build. The report identifies `build_source` as `steam_manifest`,
`preparation_receipt` or `unknown`; malformed, stale or conflicting metadata is
reported as `metadata_error` and cannot match the catalog. Receipt inspection is
not full-copy verification: use `--verify` to check every prepared asset. A matching
receipt does not authenticate ownership or establish profile/network isolation.

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

## Prepare an independent current-game copy

`CommunityDiversInspect.exe` inventories the installed Steam depots and creates a
new, independent copy for adapter development. It does not modify or start the
game, redirect services, patch protection or configure a mod loader. Preparing a
copy leaves game launch unavailable.

First, inspect the proposed inventory:

```console
CommunityDiversInspect.exe --plan --source "C:/Games/Steam/steamapps/common/Helldivers 2" --depot-cache "C:/Games/Steam/depotcache" --output copy-plan.json
```

Use the `depotcache` directory belonging to the Steam client that installed the
game, even when the game is in a separate library. Every depot manifest referenced
by `InstalledDepots` in `appmanifest_553850.acf` must be present locally with
readable filenames. Missing/encrypted manifests fail with no download or key
request. Steam must report a fully installed game. Keep Steam closed during copy
preparation so updates do not conflict with its read locks.

The plan reports the build, depot IDs, manifest IDs/digests, expected byte count
and content inventory. It checks file presence and sizes; `--copy` and `--verify`
also check every file's content. Account identifiers and the source path are
excluded from reports. Files absent from the depots, including stray mod patches,
plugins and development workspaces, are not copied. Shared Steam redistributable
depots are not bundled; the normal installed runtime prerequisites remain required.

To create a new copy on a drive with enough space:

```console
CommunityDiversInspect.exe --copy --source "C:/Games/Steam/steamapps/common/Helldivers 2" --depot-cache "C:/Games/Steam/depotcache" --destination "D:/CommunityDivers/client"
CommunityDiversInspect.exe --verify --destination "D:/CommunityDivers/client" --output copy-verification.json
```

The destination must be outside Steam libraries and must not overlap the source.
The tool never overwrites an existing destination or staging directory. Files are
copied normally, without hardlinks, symlinks or junctions. Existing path ancestors
are locked against replacement, reparse points are refused, and resolved directory
identities are checked for overlap. Ambiguous Windows path spellings are refused. Installed metadata
and depot manifests stay read-locked during preparation. Each output is flushed
and checked against its depot's SHA-1. The four diagnostic modules additionally
receive SHA-256 fingerprints. SHA-1 here is Steam's legacy content-integrity field,
not a publisher signature or proof of ownership. Cached manifests are local input;
this tool does not authenticate their signatures or contact Steam.

Work remains under `<destination>.preparing` until all files and the preparation
receipt have been written successfully. A final rename publishes the completed
copy. A failed or interrupted run retains its staging directory for diagnosis and
never creates the requested completed destination. This first implementation does
not resume a retained stage; inspect/remove that directory explicitly or choose a
new destination before retrying. There is no automatic deletion or repair.

The receipt is `.community-divers/preparation.json`. Verification rereads every
listed file, rejects hardlinked files, unexpected files and reparse points, and
compares the module fingerprints. This receipt establishes file-copy integrity at
the time of the check. It is not a trusted adapter or a game-start authorization.
Separate local user data, Steam Cloud isolation, official-network isolation and
current-client startup remain unverified and are recorded as false in reports.

The local manifest reader implements the framing and file/metadata fields described
by [SteamKit's manifest implementation](https://github.com/SteamRE/SteamKit/blob/master/SteamKit2/SteamKit2/Types/DepotManifest.cs)
and the [content manifest schema](https://github.com/SteamDatabase/Protobufs/blob/master/steam/content_manifest.proto).
It accepts only the bounded, already decrypted format needed for local preparation.
