# Community Divers project scope

Community Divers is an independent, community-operated campaign platform and
native launcher for owned copies of HELLDIVERS 2. Players share a persistent war
and progression on their chosen community service, with player-hosted missions
and optional mods. It does not connect modified gameplay to official services.

This file is the editable source of truth for scope, priorities and acceptance
criteria. Changes to the project belong here so the long-running development
objective can stay stable while the roadmap evolves.

## Product commitments

- A HELLDIVERS 2-inspired interface: broad menu navigation, blue-black panels,
  condensed headings, white typography and clear yellow selection states.
- A polished C++20 desktop launcher; application components should remain C++
  where practical. Windows/Steam is the first supported target.
- Separate application, patch-note and mod-loader modules under `src/`, with
  explicit public interfaces and independent build/test targets. Keep backend,
  update, setup and client-preparation code in their own components.
- A current-game client integration, isolated installation and separate profiles.
  The pre-EAC game is reference material, not the target implementation.
- Human-readable notes for every release, rendered inside the launcher with text
  emphasis, headings, lists and optional images, and published with the release.
- A simple per-user Windows launcher installer/uninstaller. Preserve user profiles,
  mods, campaign databases and game copies when uninstalling.
- Automatic, CI-gated launcher/client updates after publishing a pushed revision.
  Each community pins a release set so players update before joining.
  Include an operator-controlled server update agent with graceful mission drain.
- A self-hostable service with persistent player progression, mission results,
  a shared war, community decisions and configurable campaign events.
- Community multiplayer: server discovery, lobbies, invitations, compatible mod
  profiles, joins/rejoins and relays that avoid player-side port forwarding.
- HTTPS for account/campaign services. Study Steam relays and a WSS/TCP-443
  alternative for gameplay; document measured limits rather than assuming parity.
- Mod-friendly community sessions without mandatory client-side anti-cheat.
  Client protection changes may be researched for the isolated community client.
  Authentication, profile ownership and service access remain separate concerns.
- Opt-in, one-way progression import when a permitted and complete data source
  exists, with partial/manual imports as a documented fallback.
- Distribute source and launcher components, not proprietary game files. Keep
  genuine game ownership and official account/progression separation intact.

- Keep the development disclosure in the launcher About view and the README
  development section. The short public description should focus on capabilities.

## Milestones and acceptance criteria

### 1. Native launcher foundation - active

- [x] CMake builds the launcher and portable core with pinned dependencies.
- [x] Separate application, patch-note and mod-loader modules, with documented
  dependencies and independent component checks.
- [ ] Visual design is coherent, resizable and keyboard navigable.
- [x] The development disclosure is accessible in the launcher About view.
- [x] Steam library discovery and manual installation selection work.
- [ ] Verify native Win32 input, the folder picker and DPI transitions on desktop.
- [x] Display detected game build and actionable readiness information.
- [x] Community endpoints can be added, selected, saved and checked over HTTPS.
- [x] Local mod manifests can be inspected and enabled per profile; deployment
  state is explicit and no untrusted mod code runs during discovery.
- [x] Settings survive restarts and invalid settings do not erase recoverable data.
- [x] CI runs core tests on Windows and Linux and builds the Windows launcher.
- [x] Public docs explain what works, building, contributions and current limits.
- [x] The first public release has a verified portable Windows artifact.
  [Launcher preview 1](https://github.com/Nnamllit1/community-divers/releases/tag/v0.1.0-preview.1)

### 2. Isolated current-client integration

- [x] Add read-only client fingerprint inspection and an observed-build catalog.
- [x] Provide native depot-based file-copy preparation and independent content verification.
  Copy preparation alone does not verify game startup, user data or network isolation.

- [ ] Establish an exact-build integration contract and compatibility manifest.
- [ ] Create and verify an isolated current-game installation and user data.
- [ ] Establish current-client startup and native integration for community use.
- [ ] Ensure the modified client cannot connect to official gameplay/progression.
- [ ] Isolate Steam Cloud data as well as the ordinary user-data directory.
- [ ] Reach the ship against the community backend and complete a solo operation.
- [ ] Keep launch unavailable when compatibility or isolation is not verified.

### 3. Shared campaign and progression service

- [x] Add a native SQLite/HTTP service with scoped community credentials, profile/
  loadout storage, operator-configured planets/templates and deduplicated solo results.
- [x] Display real persisted campaign snapshots in the launcher.
- [x] Import community player credentials, remember sign-in securely and display live profiles.
- [x] Verify process restart continuity and the initial native backup/restore path.
- [x] Publish a versioned launcher/service contract and a self-hosting guide.
- [ ] Persist accounts, profile ownership, loadouts, progression and mission results.
- [ ] Define authoritative war state: planets, factions, operations, liberation,
  offensives, schedules and community-controlled events using existing content.
- [ ] Provide operator tools and community decision/voting controls.
- [ ] Validate and deduplicate mission contributions; make campaign rules explicit
  without requiring client-side anti-cheat.
- [ ] Test save/load, migrations, backups and campaign continuity across restarts.
- [ ] Keep each independently hosted campaign separate unless shared authority or
  federation is deliberately implemented.

### 4. Community multiplayer

- [ ] Create/list/join lobbies and advertise the required client/mod profile.
- [ ] Establish a two-machine session with compatible community clients.
- [ ] Complete a mission together and persist its campaign contribution once.
- [ ] Handle late joins, disconnects, rejoin and host exit with clear behavior.
- [ ] Verify no player-side port forwarding is required.
- [ ] Measure latency, loss behavior and bandwidth for the selected relay transport.
- [ ] Document the strict TCP-443 option and its limits, if implemented.

### 5. Mod ecosystem, import and release readiness

The [coordinated update design](docs/updates.md) defines channels and release sets.

- [ ] Install/update/remove mods transactionally in the isolated game copy.
- [ ] Detect incompatible profiles before joining rather than during gameplay.
- [ ] Provide optional progression snapshot import with a preview and field mapping.
- [x] Authenticate a signed update channel and stage size/hash-verified package downloads.
- [x] Install launcher-owned files with process-instance handoff, startup checks,
  rollback, interrupted-update recovery and persisted channel replay protection.
- [x] Publish immutable launcher previews and a signed channel after Windows/Linux CI.
  [Update-capable preview](https://github.com/Nnamllit1/community-divers/releases/tag/preview-0039e8e3846f)
- [ ] Extend publication to client-adapter/server bundles in a compatible release set.
- [ ] Check for launcher/client updates at startup and before joining a community.
- [ ] Enforce the server's required release set, including adapter and game build.
- [ ] Stage updates, apply them at a safe idle point, verify startup and roll back failures.
- [ ] Provide a server update agent that drains active missions before switching versions.
- [ ] Preserve profiles, campaign data and mods across client/server updates.
- [x] Render formatted per-release notes, including optional images, inside the launcher.
- [x] Publish the same release notes with each versioned GitHub release.
- [x] Provide and verify a Windows installer, app-list entry, shortcut and safe uninstaller.
- [ ] Finish repair, uninstall and release packaging.
- [ ] Verify the complete install -> join -> mission -> progression -> restart flow.

## Working order

Finish a usable launcher foundation first. Prepare client integration and the
service contract next; evolve the campaign and multiplayer services alongside
that integration. Client startup is an unresolved engineering gate, not an
assumption that a server URL or one skipped initialization call solves it.

Record actual evidence in acceptance checkboxes and linked development docs.
Distinguish source inspection, builds, tests, installed components and live
gameplay. Do not claim a working game patch, server or campaign from UI alone.

Use `main` for ongoing development. Keep resources, credentials, proprietary
assets, local installations, captures and build outputs out of Git.
Keep project sources and tools in this checkout. Store private design prompts
and scratch material in the ignored `.local/` directory.

## Definition of done

A new player can build or download the launcher, use an owned supported current
game copy, choose a community service, configure a compatible mod profile, join
another community player, complete a mission and see their independent personal
and shared campaign progress persist after restart. Published updates reach clients
and server operators through compatible release sets and recoverable installation.
Operators can host, manage
and back up the service with documented requirements and tested recovery.
