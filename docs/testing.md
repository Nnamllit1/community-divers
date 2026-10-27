# Validation

Run `Build.cmd` on Windows, or the Linux core presets described in the README.
CTest runs these suites:

- `core_tests`: Valve KeyValues, installation discovery, endpoint parsing,
  settings persistence/recovery and launch readiness.
- `client_identity_tests`: prepared-copy build provenance, current binary matching,
  malformed/stale receipts, metadata conflicts and unchanged game-launch readiness.
- `mod_loader_tests`: independent manifest discovery and duplicate-ID rejection.
- `patch_notes_tests`: immutable revision lookup, document/image limits, safe relative
  image paths and an injected asynchronous transport without updater/UI dependencies.
- `update_core_tests`: signed-payload fields, package bounds, release URL/path
  constraints, known/unknown revision ordering and compatible settings migration.
- `update_install_tests` (Windows): real disposable launcher/helper processes,
  successful replacement, failed/hung startup rollback, unsafe ZIP/signature
  rejection and recovery after a mid-transaction process crash. User data and
  unmanaged files must survive each case. Fixture executables are not packaged.
- `update_tests` (Windows): real CNG signatures with generated fixture keys,
  tampering/wrong-key rejection, transport URL policy, streamed Unicode staging,
  verified cache reuse/repair, failed-transfer cleanup and persisted replay rejection.
- `service_tests` (Windows/Linux): real loopback HTTP, private state creation,
  operator/player separation, persistent loadouts, declared-version admission,
  snapshotted rewards, duplicate/conflicting results, cross-connection concurrent
  retries, capped liberation, online backup/restore, restart continuity and
  unrelated/forward-schema rejection. Windows also probes the service with the
  launcher's actual WinHTTP client; Linux checks owner-only file permissions.
- `note_transport_tests` (Windows): the application transport streams binary note
  images above the update manifest size limit without truncation or text conversion.
- `setup_tests` (Windows): installation ownership, interrupted setup repair, path
  rejection, active-launcher locks and inventory-based removal that preserves
  modified resources and unrelated files.
- `platform_tests` (Windows): real loopback HTTP requests, base-path handling,
  redirects, HTTP errors, invalid JSON, response limits and SHA-256 fingerprints.
- `ui_tests` (Windows): the actual launcher views in an offscreen ImGui context.
  Input events exercise section shortcuts, text focus, manual game paths,
  community forms and checks, mod checkboxes and profiles, restart persistence,
  stale network responses, asynchronous client inspection and the minimum-window
  footer/navigation layout, update-check preferences, verified-release display,
  staged-download results, authentication errors and installation handoff errors/success.
  The real patch-note renderer also exercises formatting, image loading/caching and
  rejection of stale asynchronous document results.

The widget suite uses a repository-owned driver through the pinned ImGui test
hooks. It runs without a GPU and keeps fixtures in a uniquely named temporary
directory. It reads synthetic installation files and never launches the game.
Native Win32 event delivery, DPI transitions and the folder picker require a
separate desktop check.

## Optional HTTPS smoke check

After building, this command exercises Windows TLS validation and status parsing
against the repository's public, static protocol fixture:

```console
out/windows-dev/platform_tests.exe --https-status https://raw.githubusercontent.com/Nnamllit1/community-divers/main/tests/fixtures
```

It is separate from offline CI and requires Internet access. The fixture reports
a synthetic build ID and is a protocol check, not a playable community service.

## Render captures

The application can export its own Direct3D backbuffer for visual review:

```console
CommunityDivers.exe --data-dir captures/qa-data --page 0 --window-size 1040 730 --capture captures/launcher.png
```

Pages are Deployment (0), Campaign (1), Communities (2), Mods (3), Settings (4)
and About (5). Use a separate data directory for test captures. A render capture
establishes appearance and startup; widget and desktop checks establish input.

## Optional public update smoke check

```console
out/windows-dev/CommunityDiversUpdate.exe --check
out/windows-dev/CommunityDiversUpdate.exe --download captures/update-download
```

The first command fetches and authenticates the public preview feed. The second
also follows the public release asset redirect and verifies the downloaded ZIP.
Both require Internet access and are separate from offline CTest. Neither installs
or executes the download. Signing tests exercise native cryptography; this public
check verifies the actual published feed/key/package combination.

## Optional real launcher startup acknowledgement

```console
cmake --install out/windows-dev --prefix captures/portable
out/windows-dev/update_launcher_smoke.exe captures/portable captures/health-data captures/health.png
```

This starts the actual packaged launcher with explicitly inherited lock and health
handles, checks the first rendered frames, exports its own backbuffer and requires
a normal process exit. It needs Windows graphics and is not part of headless CI.
It sends no desktop input and does not replace native input/dialog/DPI validation.

## Publishing and actual launcher upgrades

The preview publishing job requires both platform jobs, verifies clean build
metadata/checksum, validates the signed archive through the native extractor and
checks published asset digests before channel promotion. Pull requests never run
the publishing job or receive its signing secret. Native `--sign-env` rejects a
secret that does not match the compiled public key before writing a feed.

`CommunityDivers.exe --update` provides the production noninteractive handoff for
upgrade checks between actual released builds; process fixture coverage remains
separate from that end-to-end result.

The release-to-release smoke check has been exercised from
`preview-bdb99ef95204` to `preview-0039e8e3846f` using the production `--update`
command and the public signed feed. The replacement passed its native startup
check and remained alive afterward; saved settings, mod metadata and unmanaged
files were preserved, and the transaction was confirmed. This is launcher update
evidence, separate from the synthetic rollback fault tests and from game startup.

## Campaign view against a running service

Save a local community endpoint in a separate launcher data directory, start the
service, then use the application's own capture mode:

```console
CommunityDivers.exe --data-dir captures/campaign-launcher-data --page 1 --check-selected --capture captures/campaign.png
```

`--check-selected` requests the saved community's actual status/campaign and waits
for that request before exporting a render capture. It sends no desktop input.
The separate service-process check also verifies persisted HTTP state after a
forced process stop/restart and exercises the native backup/restore CLI. These
checks do not establish external TLS deployment, native input/DPI behavior or a
working game adapter.

The initial service has passed both Windows and Linux CI, with separately installed
service artifacts. A Windows process check initialized private state on a Unicode
path, made authenticated HTTP changes, force-stopped/restarted the service and
verified profile/campaign continuity and duplicate-result protection. Native
backup/restore commands also passed while the service was running. The launcher
render check fetched that persisted campaign through its production WinHTTP path.
External HTTPS hosting and native game sessions remain unverified.

## Native client-copy preparation

`preparation_tests` checks bounded local depot parsing on Windows/Linux. Windows
also tests the real filesystem/BCrypt path: Unicode directories, independent file
copies, stray-mod exclusion, source/destination corruption, existing destinations,
Steam/source overlap, NTFS junction rejection, hardlink detection, installed
metadata read locks and interrupted-copy staging. Fixtures contain only synthetic
`abc` file content and fabricated depot metadata. No Steam account, game launch,
network connection or proprietary fixture is needed.

The inspector's `--plan`, `--copy` and `--verify` commands are documented in
[client integration](client-integration.md). A read-only plan of the installed
build 26088801 resolved its three local depot manifests into 168 game files totaling
24,150,575,918 bytes. This establishes inventory parsing/presence/size checks on
that installation. A complete independent HDD copy of those 168 files has also
passed the native copy and separate verification commands, including all depot
content hashes and the four catalogued module fingerprints. No game startup or
profile/Steam Cloud/official-network isolation is established by that copy.
Synthetic fixtures cover interruption, corruption and path-rejection failures.

## Community profile sign-in

Core checks cover bounded credential/profile parsing, identity mismatch, malformed
credentials and deep JSON rejection without secret fragments in errors. Native
Windows checks write/read/delete an application-scoped Credential Manager fixture
and confirm separation by data directory, community URL and identity. Real WinHTTP
requests exercise successful profile reads against the service, invalid credentials,
wrong player IDs, public status without authorization, rejected redirects, response
limits and credential-safe error handling.

Offscreen widget tests exercise credential-file import, remembered/session-only
sign-in, refresh, local sign-out and discarding responses after community changes.
Native dialog input, public authenticated HTTPS deployment and game account/mission
integration remain separate verification requirements.

A native launcher render check has restored a scoped Windows credential and read
the real service profile across fresh launcher processes and a forced service
restart. Normal and minimum-size captures show the persisted XP, completion count
and saved-loadout count with visible primary actions. The QA credential was seeded
through the Windows API and removed afterward; the file dialog was not driven by
that process check. This is profile/HTTP/render evidence, not native gameplay.

## Setup and native patch-note checks

Windows CI builds a QA setup with a separate registration name, then runs
`tests/setup_smoke.py` against it. The check installs to a Unicode path, verifies
the app-list entry and shortcut, starts the installed native launcher, refuses
an active-launcher uninstall, and uninstalls while preserving custom files,
mods, a synthetic game copy and separate user data. It refuses to replace any
existing test registration. This exercises the real install/uninstall commands;
interactive wizard input remains a separate desktop check.

To capture the native note viewer from packaged docs:

```console
CommunityDivers.exe --data-dir captures/notes-data --page 5 --release-notes --capture captures/release-notes.png
```

The viewer is an ImGui window rendered by the launcher, not an external browser.
Use the normal window-size option to inspect wrapping at the minimum supported
size. Formatting tests use an offscreen context; native backbuffer capture also
exercises the real Direct3D/WIC image path.

A live native HTTPS check also fetched an exact public source revision and its
128,316-byte inline PNG through the application transport. That is separate from
the deterministic stream/formatting tests and from local image render captures.

The [modular launcher release](https://github.com/Nnamllit1/community-divers/releases/tag/preview-2d5db4129dce)
passed Windows and Linux CI. Its public ZIP passed native signature/inventory
verification; the public setup passed installation, app-list/shortcut checks,
real launcher note rendering and removal while retaining custom files and user
data. Release-note bytes match between the ZIP and the published Markdown asset.
The public installer check used its silent command interface; interactive wizard
input and DPI behavior are still separate desktop checks.

## Interrupted setup repair

`tests/setup_repair_smoke.py` starts a separately named QA installer that pauses
after writing its first executable, then terminates that exact process through
its retained process handle. The normal installer repairs the interrupted state.
The check covers a new installation and reinstall over a registered copy, refuses
launcher startup while setup is incomplete, verifies native startup after repair,
and removes the repaired installation while retaining custom mod files.

The pause is compiled only with `tools/build-setup.ps1 -TestIdentity -TestInterrupt`;
it is absent from production installers. CI builds the normal and interruption
fixtures in separate artifact folders and never publishes either QA installer.
This process-level check does not automate installer wizard input.

The setup-repair release `preview-95e62b2487e7` passed Windows and Linux CI,
including both real interrupted-install recovery cases. The downloaded public
ZIP passed native signature/inventory verification; its setup/ZIP SHA-256 values
and packaged release notes matched the published assets.

## Prepared-copy identity

The native inspector also identified an existing independently prepared build
`26088801` through its receipt, with all four current binary hashes matching the
observed catalog. Its report retained `integration.available: false`. This check
read only the four diagnostic modules and receipt; it was not a repeat of full
asset verification or a game-start test. Portable identity tests cover malformed,
stale and conflicting receipts. Windows preparation tests exercise receipts made
by the real copy tool, and widget tests verify the copied build reaches the
launcher display and is cleared when the installation changes.

The prepared-copy release [preview-dfe5c7b80b5d](https://github.com/Nnamllit1/community-divers/releases/tag/preview-dfe5c7b80b5d)
passed Windows and Linux CI. Its downloaded ZIP passed native signature/inventory
verification, the setup/ZIP hashes matched their published checksums, and its
shipped inspector identified the prepared build through the receipt while keeping
integration unavailable.

The [current-client bootstrap probe](client-startup.md) documents a separate
runtime process and the conditions of an offline startup observation. It did not
reach engine startup and does not satisfy the adapter acceptance gates.

## Standalone GPL loader checks

`integrations/hellforge-loader/Build.ps1` builds the pinned native loader and runs
the two required synthetic suites, without loading a real game. It writes a
successful verification receipt only after both pass, invalidating any previous
success before configuring. See the
[component instructions](../integrations/hellforge-loader/README.md) for prerequisites,
outputs, license boundaries and the unverified current-game integration.
