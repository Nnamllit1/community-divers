# Validation

Run `Build.cmd` on Windows, or the Linux core presets described in the README.
CTest runs these suites:

- `core_tests`: Valve KeyValues, installation discovery, endpoint parsing,
  settings persistence/recovery, manifest validation and launch readiness.
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
- `platform_tests` (Windows): real loopback HTTP requests, base-path handling,
  redirects, HTTP errors, invalid JSON, response limits and SHA-256 fingerprints.
- `ui_tests` (Windows): the actual launcher views in an offscreen ImGui context.
  Input events exercise section shortcuts, text focus, manual game paths,
  community forms and checks, mod checkboxes and profiles, restart persistence,
  stale network responses, asynchronous client inspection and the minimum-window
  footer/navigation layout, update-check preferences, verified-release display,
  staged-download results, authentication errors and installation handoff errors/success.

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
