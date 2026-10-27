# Standalone HellForge loader component

This Windows x64 component builds the pinned HellForge native loader separately
from the Community Divers launcher and campaign service. It is a development
integration target, not a deployable current-game adapter. The application does
not load it, and normal launcher packages do not contain it.

## Build and verify

From the repository root, with Visual Studio's CMake tools installed:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File integrations/hellforge-loader/Build.ps1
```

Alternatively pass `-CMake <cmake.exe>` and `-Ninja <ninja.exe>` to the script.
CMake/CTest 3.25 or newer is required. The script downloads the checksum-pinned
Zig 0.17.0 compiler into `.local/tools/hellforge-loader/`; nothing is installed
system-wide. CMake downloads the pinned upstream sources and MinHook 1.3.4 into
the standalone build directory. No upstream PowerShell scripts, session runner,
Steam ownership code, backend or game startup path is executed.

The dependency versions, archive hashes and upstream game build are recorded in
[upstream.lock.json](upstream.lock.json). Build outputs are under
`out/hellforge-loader/`, including:

- `bin/hellforge_modloader.dll`: the separately built native library;
- `licenses/`: upstream GPL, mod exception, SDK and MinHook notices;
- `build-info.json`: upstream revision, source digest and compiler identity;
- `verification.json`: compiler/library hashes and the names of completed tests;
- `smoke-*`: retained synthetic fixture logs.

`verification.json` starts as `unverified` on every invocation and changes to
`verified_synthetic` only after both named tests pass. A failed build cannot leave
a previous success marked current. Its
`current_client_compatible` and `deployable` fields remain false. The source digest
records the fetched loader/header contents used by CMake; it is not a signature or
proof of a pristine local dependency tree. Modifying a cached source tree requires
reconfiguration/rebuilding and review of the resulting changes.

## What the tests establish

`hellforge_synthetic_stats` builds upstream's stats, registry and related fixture
checks against synthetic structures. `hellforge_loader_smoke` loads the real
loader into a dedicated test process, loads two synthetic native mods against a
fake game DLL, and checks event delivery, pattern lookup, original-code validation
and chained hooks. Neither test starts, attaches to or changes a real game.

The upstream loader currently owns process-lifetime worker threads and exposes no
unload protocol. The smoke process exits after its checks rather than unloading
live hooks. Production integration must use controlled process lifetime and a
restart for native-profile changes; these tests do not establish hot unloading.

A passing build does not establish support for game build 26088801. Upstream's
reviewed build remains 25480438. Native layouts, hook sites, startup dependencies,
profile isolation and the player-owned approval path still need integration and
verification. See the [component evaluation](../../docs/hellforge-evaluation.md)
and [mod trust design](../../docs/mod-trust.md).

## License and distribution

This directory's build integration and test harness are GPLv3 under [LICENSE](LICENSE),
with the additional mod permission in [LICENSE-EXCEPTION](LICENSE-EXCEPTION).
Community Divers grants that same additional permission for its own modifications
and harness here. The fetched HellForge loader retains its upstream notices;
its SDK is MIT-0 under [LICENSE-SDK](LICENSE-SDK). MinHook retains its BSD-2-Clause
license. None of these terms applies gambling/monetization restrictions to GPL code.

The component has no install/package target and is deliberately excluded from the
MIT launcher's default distribution. Before distributing a modified loader binary,
provide the corresponding source, modifications, build material and required
notices under the applicable GPL terms; an upstream link alone is not a complete
source delivery for a modified binary. Local build outputs are not a release.
