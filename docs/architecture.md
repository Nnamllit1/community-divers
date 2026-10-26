# Architecture

Community Divers separates the desktop launcher, current-game integration and
community service. [PROJECT.md](../PROJECT.md) defines their milestones.

## Launcher

- `community_core`: installation metadata, Valve KeyValues parsing, settings,
  server endpoints, service status, mod profiles, exact-file identity matching and
  launch-readiness decisions and update manifest validation.
- `community_platform`: Windows Steam discovery, HTTPS requests, read-only BCrypt file hashing, ECDSA publisher verification, streamed update staging and desktop paths.
- `CommunityDivers`: Win32/Direct3D 11 desktop application and Dear ImGui views.

The launcher displays actual detected and configured state. There is no built-in
community server and no simulated campaign presented as a live one. The initial
client integration capability is unavailable, so a remote service claiming to
support a build cannot enable game launch by itself.

Settings and mod manifests stay in the launcher's own user-data directory. Merely
discovering an installation never changes the game, its settings or Steam Cloud.

## Planned game integration

A separately versioned adapter will manage an isolated current-game copy,
private profile storage, service routing and the client startup contract. A
supported game build, verified isolation and a compatible community service will
all be required before launch. Startup and native access remain unverified.

## Planned community service

One authoritative campaign service owns its war and player progression. It
provides accounts, mission/operation data, lobbies, community decisions and
deduplicated mission results. Independent services have independent campaigns
unless shared authority/federation is explicitly added.

Player-hosted missions retain the game's simulation. HTTPS handles service data;
gameplay needs a tested relay transport. Steam relay and secure WebSocket/TCP 443
are separate transport options with different integration and latency costs.

Mod support does not imply every mod combination is network-compatible. Lobbies
will advertise their required client and gameplay mod profile before joining.

The [client inspection contract](client-integration.md) separates observed binary
identity from adapter availability. Identity reports never grant game launch.

## Updates

The [update design](updates.md) links CI publication to immutable client/server
release sets, admission checks, idle-point installation and operator-controlled
server rollout. Source builds check a signed feed and stage verified downloads.
Installation and server coordination remain pending. The update trust root is
compiled into the launcher rather than provided by a community endpoint.
