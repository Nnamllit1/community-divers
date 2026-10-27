# HellForge component evaluation

Reviewed on 27 October 2026 at commit
[`b1aef771474710eab5461d360444cd80a809c70c`](https://github.com/SkyeShade/HellForge/tree/b1aef771474710eab5461d360444cd80a809c70c).
The initial review was static. A separate
[GPL component build](../integrations/hellforge-loader/README.md) now compiles the
pinned native loader and runs synthetic tests. No upstream session/build script
or game-startup code was executed. The loader is not deployed into the game or
included in the launcher package.

## License and reuse decision

| Component | License at the reviewed commit | Integration consequence |
| --- | --- | --- |
| Native loader and other non-SDK project code | GPLv3 with HellForge's mod exception | A reused or modified loader retains those terms and its corresponding-source obligations |
| SDK, templates and example mods | MIT No Attribution (MIT-0) | These can be reused and adapted under the stated permissive grant |
| Independently written Community Divers backend | Separate licensing decision | Its terms must not purport to restrict GPL code or previously granted MIT rights |

The exact scope is in upstream
[LICENSE-SDK](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/LICENSE-SDK),
[LICENSE](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/LICENSE)
and [LICENSE-EXCEPTION](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/LICENSE-EXCEPTION).
The exception allows qualifying independent mods to use their authors' own terms;
it does not relicense the loader. Older indexed descriptions of a closed engine
are not the license or source state at this reviewed commit.

The selected direction is to reuse the GPL loader where compatible, preserving
the mod exception, and keep Community Divers' independently written services
separate. Gambling/monetization restrictions must not be added to the GPL loader.
A process boundary alone is not a substitute for reviewing derivative-work and
linking obligations. Importing GPL backend code would require a new compatibility
review; merely translating that code to C++ would not remove its license.

## Current-build limitation

The checked-in
[supported-build metadata](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/src/HellForge.Core/supported-build.json)
selects Steam build **25480438**. Community Divers' prepared current-client target
is **26088801**. HellForge's ability to prepare its supported older build from a
newer installation is not proof of native support for the newer executable.

The loader is C with native hooks. Its
[engine architecture](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/docs/engine.md)
also includes a session runner, local backend and SDK/Cloud stand-ins. The loader
is therefore not a standalone solution to Community Divers' unresolved current
bootstrap. Exact-build tables, hook sites, component layouts, startup callbacks
and bridge dependencies need review before integration. Do not substitute the
older client for the current-client milestone or enable launch based on this review.

## Native mods and required profiles

In
[`SetupMod`](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/native/mod-loader/hellforge_modloader.c#L1038),
HellForge loads the selected DLL through `LoadLibraryExW` and calls its exported
initializer. The SDK is an interface, not a security boundary around that code.
The earlier DLL initialization can also run before the SDK entrypoint is checked.

The
[mod-sync planner](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/src/HellForge.Core/ModSync.cs)
separates content, code, raw and bundled packages. It checks exact content hashes
against verified or locally trusted entries and distinguishes downloads from
manual installs and trust requests. File integrity and package classification are
useful designs to reuse, not evidence of a sandbox.

Its [multiplayer trust documentation](https://github.com/SkyeShade/HellForge/blob/b1aef771474710eab5461d360444cd80a809c70c/docs/multiplayer.md#trust-what-another-players-mods-can-reach)
explicitly states that native code can still access files and other same-user
programs outside held folders. Network and child-process restrictions limit some
actions but do not make arbitrary native mods safe. Source distribution and a
pinned compiler likewise do not establish benign behavior.

Community Divers' proposed requirement/approval boundary is specified in
[Community-required mods and player trust](mod-trust.md). Required profiles can
be compared before joining without automatically downloading or executing native
code supplied by a host.

## Adoption gates

1. Pin and verify the selected source revision and dependency licenses; preserve
   GPL notices, the mod exception, source/build instructions and modifications.
2. Define the narrow adapter interface and identify all old-build dependencies.
   Keep GPL-derived implementation separate from independently written service code.
3. Build and test against synthetic fixtures before using the isolated current
   client. Successful compilation is not proof that old-build hooks work.
4. Verify the current bootstrap, isolation and exact-build adapter contract before
   enabling runtime deployment.
5. Implement the player-owned mod approval and profile admission rules before
   offering any community-driven native-mod setup.

## Standalone build evidence

The pinned Zig/MinHook build passed both named CTest suites locally on Windows:
upstream synthetic stats/registry fixtures and the separate two-mod loader smoke
check. The latter verifies actual DLL loading, event delivery, pattern lookup and
chained hooks in a dedicated process with a fake game DLL. The build retains the
GPL/mod-exception/SDK/MinHook notices and writes source/compiler/library identity
receipts. These results establish the component build, not current-game runtime
compatibility or multiplayer behavior.
