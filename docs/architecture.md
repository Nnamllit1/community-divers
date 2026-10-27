# Architecture

Community Divers separates the desktop launcher, current-game integration and
community service. [PROJECT.md](../PROJECT.md) defines their milestones.

## Source modules

One repository holds the components together; each module has its own source
folder and CMake target. Shared contracts live in `include/community/`. The
application composes these interfaces instead of embedding their implementations.

| Source folder | Target / responsibility |
| --- | --- |
| `src/application/` | `CommunityDivers`: desktop window, navigation, views and orchestration |
| `src/patch_notes/` | `community_patch_notes`: bounded documents and asynchronous retrieval; `community_notes_view`: formatted ImGui rendering |
| `src/mod_loader/` | `community_mod_loader`: manifest discovery, validation and duplicate detection |
| `src/shared/` | `community_core`: paths, settings, service data contracts and readiness decisions |
| `src/client/` | `community_client`: exact-build identity and depot parsing; `CommunityDiversInspect`: inspection/copy CLI |
| `src/platform/` | `community_platform`: Windows paths, dialogs, credentials, HTTP, hashing and native client-copy preparation |
| `src/updates/` | `community_update_core`: payload/inventory validation; `community_maintenance`: signed transport, staging and update transactions |
| `src/installer/` | Setup ownership, verified removal and app-list registration, integrated into `community_maintenance` |
| `src/service/` | `community_service` and `CommunityDiversService`: independent SQLite/HTTP campaign backend |

`installer/` at the repository root holds the NSIS wizard, while `tools/` holds
packaging/publication commands. Release content is `docs/release-notes.md`, separate
from its renderer. `tests/` owns the CTest executables. Binaries still go to the
preset's build directory, so packaging paths and existing update clients stay stable.

### Boundaries

- Patch-note retrieval receives a transport callback. It does not know about
  update installation, the launcher window or the mod loader. The application
  supplies the bounded HTTPS transport and selects the exact release revision.
- The note renderer accepts a document, fonts and an image-loading callback. The
  application owns navigation, GPU resources and their lifetime. The same renderer
  builds against production ImGui and the offscreen widget-test configuration.
- Mod discovery has a dedicated `mod_loader.hpp` interface. Settings share only
  profile data through `mod_types.hpp`; scanning never executes a mod. Native mod
  loading and transactional deployment remain unimplemented.
- The service links the shared contracts and its storage/HTTP dependencies. It
  does not depend on application, patch-note, mod-loader or updater code.
- Setup and portable updates deliberately share locks, inventory rules and
  ownership transactions in `community_maintenance`. They use separate entry
  points without duplicating removal/update policy.

Keep cross-module calls on these public interfaces; do not include another
module's implementation files or application state. New features belong with
the component that owns their behavior. Independent targets can be built with
`cmake --build --preset windows-dev --target community_patch_notes community_mod_loader`.

## Launcher

The launcher displays detected/configured state and campaign snapshots fetched
from a real community service. Empty campaigns remain empty until configured. The initial
client integration capability is unavailable, so a remote service claiming to
support a build cannot enable game launch by itself.

Settings and mod manifests stay in the launcher's own user-data directory.
Player sign-ins use scoped Windows Credential Manager entries; authenticated
profile requests verify community/player identity and never follow redirects. Merely
discovering an installation never changes the game, its settings or Steam Cloud.

## Planned game integration

A separately versioned adapter will manage an isolated current-game copy,
private profile storage, service routing and the client startup contract. A
supported game build, verified isolation and a compatible community service will
all be required before launch. Startup and native access remain unverified. The
native preparation tool can copy only installed depot files into a new directory
and verify their content; this file-copy layer does not establish profile, Cloud
or network isolation.

## Community service

The native `CommunityDiversService` owns its independent community state in SQLite.
It provides scoped operator/player credentials, profile/loadout storage, configured
planets and mission templates, transactional solo result receipts and native backup/
restore. Lobbies, operations, scheduling and community decisions remain planned. Independent services have independent campaigns
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
A separate native helper installs owned launcher files, verifies restart health
and restores failed/interrupted updates. Server coordination and automatic
scheduling remain pending. The update trust root is
compiled into the launcher rather than provided by a community endpoint.

See [self-hosting](self-hosting.md) for the explicit Server install component,
loopback/HTTPS-proxy deployment and the boundary between API tests and game integration.
