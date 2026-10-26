# Validation

Run `Build.cmd` on Windows, or the Linux core presets described in the README.
CTest runs these suites:

- `core_tests`: Valve KeyValues, installation discovery, endpoint parsing,
  settings persistence/recovery, manifest validation and launch readiness.
- `platform_tests` (Windows): real loopback HTTP requests, base-path handling,
  redirects, HTTP errors, invalid JSON and response limits.
- `ui_tests` (Windows): the actual launcher views in an offscreen ImGui context.
  Input events exercise section shortcuts, text focus, manual game paths,
  community forms and checks, mod checkboxes and profiles, restart persistence,
  stale network responses and the minimum-window footer/navigation layout.

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
