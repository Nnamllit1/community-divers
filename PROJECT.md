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
- A native C++ per-user Windows installer/uninstaller styled like the launcher,
  replacing the legacy installer interface. Preserve user profiles,
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
- Communities may require exact compatible mod profiles, but a remote requirement
  must not authorize native-code execution. Follow the [mod trust design](docs/mod-trust.md):
  locally validated data, bounded sandboxed logic and separate approval for native mods.
- Reuse HellForge's GPL loader where compatible, preserving its mod exception;
  keep independently written backend/service code separate. Follow the
  [component licensing](docs/licensing-plan.md). The adopted
  [Service License 1.0](docs/licenses/community-divers-service-1.0.md) requires source
  sharing, permits unlimited genuine donations and decorative ranks with paid
  monthly revenue capped at twice actual monthly operating costs, and prohibits
  gambling and paid advantages. Earlier MIT grants remain intact. Paid pets and
  other cosmetics remain a future policy option.
- Mod-friendly community sessions without mandatory client-side anti-cheat.
  Client protection changes may be researched for the isolated community client.
  Authentication, profile ownership and service access remain separate concerns.
- The community client must not require players to install a custom kernel driver,
  enable test-signing or disable driver-signature enforcement. Investigate a
  reproducible user-mode integration for each supported game build; a binary
  patch is a candidate delivery method, not an established startup solution.
- Opt-in, one-way progression import when a permitted and complete data source
  exists, with partial/manual imports as a documented fallback.
- Distribute source and launcher components, not proprietary game files. Keep
  genuine game ownership and official account/progression separation intact.

- Keep the development disclosure in the launcher About view and the README
  development section. The short public description should focus on capabilities.

## Milestones and acceptance criteria

### 1. Native launcher foundation

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

### 2. Isolated current-client integration - current priority

- [x] Add read-only client fingerprint inspection and an observed-build catalog.
- [x] Provide native depot-based file-copy preparation and independent content verification.
  Copy preparation alone does not verify game startup, user data or network isolation.

- [x] Build the selected GPL HellForge loader separately and verify its startup,
  mod callbacks and hook chaining against synthetic fixtures. This does not
  establish current-game compatibility.
- [ ] Establish an exact-build integration contract and compatibility manifest.
- [ ] Create and verify an isolated current-game installation and user data.
- [ ] Establish current-client startup and native integration for community use.
  [Bootstrap investigation](docs/client-startup.md) identified a separate runtime
  process; engine startup remains unverified.
  [Driver-free integration study](docs/client-protection-study.md) distinguishes
  historical protection research from evidence for the supported game build.
- [ ] Verify the client integration on a second machine without custom drivers,
  researcher-installed tooling or development-machine state.
- [ ] Ensure the modified client cannot connect to official gameplay/progression.
- [ ] Isolate Steam Cloud data as well as the ordinary user-data directory.
- [ ] Reach the ship against the community backend and complete a solo operation.
- [ ] Keep launch unavailable when compatibility or isolation is not verified.

#### Current-client bootstrap work package

This is the immediate development focus. Further launcher polish and unrelated
features wait until the current client can run with a community-owned campaign.
Start with the current build's protection/bootstrap architecture: determine what
the runtime supplies to both the executable and engine, then establish whether
a reproducible driver-free replacement is feasible. AppContainer compatibility
is not a prerequisite for this investigation or for the selected mod loader.
Further AppContainer adaptation is deferred; it must not displace the bootstrap
investigation. The eventual integration still requires verified data and official
service separation, regardless of which isolation mechanism is selected.
Prefer a reproducible, version-specific binary patch applied to the player's
isolated copy. If a user-mode runtime adapter is also necessary, identify and
document that dependency rather than claiming that an on-disk patch is sufficient.

- [x] Attribute the observed `STATUS_ACCESS_DENIED` to a specific operation and
  resource. Distinguish probe restrictions from protection/runtime dependencies
  using bounded diagnostics with verifiable cleanup.
  The [named-mutex comparison](docs/client-startup.md#named-object-compatibility)
  identifies an AppContainer namespace incompatibility, not a verified integrity
  rejection. A subsequent diagnostic comparison advanced the real client beyond
  this request, but has not reached the engine.
- [ ] Map the required startup behavior of the main executable, game DLL,
  preloader and runtime for the exact supported build. Identify protected-code
  preparation, imports, TLS callbacks, exception handling and any continuing
  runtime/process/device dependencies that affect a replacement.
  The [static bootstrap map](docs/client-protection-study.md#current-build-startup-structure)
  identifies shared TLS/import slots and exception-metadata differences. This
  establishes concrete dependencies; it does not identify all runtime work.
  The [preloader handoff](docs/client-protection-study.md#preloader-to-runtime-handoff)
  now explains its exception-mediated entry and cached runtime dispatch. The
  runtime's image preparation and continuing dependencies remain unresolved.
  Both examined stock engine plugins also import the preloader; their required
  startup preparation and role in the working client remain unverified.
  [Bounded runtime observation](docs/client-protection-study.md#initial-runtime-state-dependency)
  follows thread/process-state access and CPU initialization without running the
  game. The [shared record](docs/client-protection-study.md#shared-initialization-record)
  supplies an additional Windows function table. With measured diagnostic inputs,
  the [table-consumption trace](docs/client-protection-study.md#runtime-table-consumption)
  copies all 18 pointers. Subsequent
  [support-module parsing](docs/client-protection-study.md#support-module-parsing)
  reaches Windows export-directory metadata using measured diagnostic inputs.
  [Export-name observation](docs/client-protection-study.md#export-name-observation)
  identifies a native-export address/name-hash table. With deterministic
  observation slices, both register cases reach the same
  [first system call](docs/client-protection-study.md#first-system-call-boundary),
  identified against measured Windows code as a zero-timeout process wait.
  A [measured wait continuation](docs/client-protection-study.md#measured-wait-continuation)
  verifies four exact operations in diagnostic-owned processes and advances
  the model to another keyed-event request. A fifth native contract is measured,
  with its actual model continuation still unverified. Post-return readbacks and
  status access traces distinguish result lifetimes from reused stack slots.
  Verify the next request and the results' consumers, then identify protected-image preparation;
  missing simulation inputs, emulator limitations and ordinary initialization
  are not evidence of a removable protection check.
- [ ] Separate protection requirements from diagnostic-environment failures.
  AppContainer's named-object failure has been identified; its later
  `0xC0000244` exit remains unattributed. Neither is proof of a kernel-driver
  requirement or a protection rejection. Do not turn sandbox compatibility into
  a product acceptance gate or broaden machine-wide permissions to mask it.
- [ ] Implement and verify the necessary current-client protection/bootstrap
  adaptation in the isolated copy, preserving working engine initialization.
  Do not assume historical offsets, keys or driver behavior apply to this build.
- [ ] Produce a repeatable patch/integration build from exact input hashes.
  Keep originals intact; verify output identity, reject unsupported inputs and
  support restoration without reinstalling the official game.
- [ ] Prove repeated cold startup through engine initialization without a custom
  driver, test-signing, disabled signature enforcement or development-only state.
- [ ] Verify separate user/Cloud data and enforce exclusion of official gameplay
  and progression endpoints throughout the adapted client's process tree.
- [ ] Connect the actual game client to the community backend and reach the ship.
- [ ] Select and launch a mission from the community campaign using installed
  game content; complete it, return to the ship and persist its contribution to
  the community campaign only. Custom map assets are not this requirement.
- [ ] Repeat the working flow on a second machine without research tools or
  custom drivers. Record the supported game hashes and reproducible procedure.

The exit criterion is the real client using a community campaign. A memory dump,
a decoded function, a passing fixture, a patched executable or a launcher screen
alone does not satisfy this work package. If a particular approach fails, record
the evidence and investigate the next viable approach within the same scope.

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
- [ ] Validate required mod packages locally and obtain separate, version-specific
  approval for native code; a community cannot grant itself execution permission.
- [ ] Verify sandbox limits, package integrity, consent revocation and refusal/rejoin
  behavior before enabling community-driven mod setup.
- [x] Adopt the independent service license with explicit component coverage,
  source/package notices and preserved earlier MIT grants.
- [ ] Complete GPL-loader distribution obligations before distributing game integration;
  do not restrict independently licensed mods.
- [ ] Provide optional progression snapshot import with a preview and field mapping.
- [x] Authenticate a signed update channel and stage size/hash-verified package downloads.
- [x] Install launcher-owned files with process-instance handoff, startup checks,
  rollback, interrupted-update recovery and persisted channel replay protection.
- [x] Publish immutable launcher previews and a signed channel after Windows/Linux CI.
  [Update-capable preview](https://github.com/Nnamllit1/community-divers/releases/tag/preview-0039e8e3846f)
- [x] Use readable numbered preview versions across releases, downloads and the
  launcher, with publication checks that skip older CI builds finishing late.
- [ ] Extend publication to client-adapter/server bundles in a compatible release set.
- [ ] Check for launcher/client updates at startup and before joining a community.
- [ ] Enforce the server's required release set, including adapter and game build.
- [ ] Stage updates, apply them at a safe idle point, verify startup and roll back failures.
- [ ] Provide a server update agent that drains active missions before switching versions.
- [ ] Preserve profiles, campaign data and mods across client/server updates.
- [x] Render formatted per-release notes, including optional images, inside the launcher.
- [x] Publish the same release notes with each versioned GitHub release.
- [x] Provide and verify a Windows installer, app-list entry, shortcut and safe uninstaller.
- [x] Replace the legacy installer frontend with native C++ setup and uninstall
  interfaces matching the launcher; retain and reverify repair, interrupted-install
  recovery, ownership checks and preservation of profiles/mods/game copies.
  Packaged setup/removal, interruption recovery, payload integrity and native screen
  captures are checked; desktop input and DPI verification remain open above.
- [x] Finish repair, uninstall and release packaging.
  [Repair-verified launcher preview](https://github.com/Nnamllit1/community-divers/releases/tag/preview-95e62b2487e7)
- [ ] Verify the complete install -> join -> mission -> progression -> restart flow.

## Working order

The launcher and native installer provide the current foundation. Prioritize
the current-client protection/bootstrap investigation now, with driver-free
player deployment as a requirement. Execute the bootstrap work package above
before additional launcher polish or unrelated features. Backend work during
this phase should directly support the actual client's community-campaign flow.
Client startup is an unresolved engineering gate, not an assumption that a server URL, a larger
binary patch or one skipped initialization call solves it.

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
