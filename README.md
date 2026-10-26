<div align="center">

<img src="assets/brand/logo.svg" alt="Community Divers" width="520">

**Your community. Your campaign.**

A native C++ launcher and self-hosted campaign platform for community-run
HELLDIVERS 2 wars.

</div>

> **Launcher preview.** Installation discovery, saved server connections and mod
> profiles are available. Game integration, campaign services and community co-op
> are under development. This preview cannot launch a community game session.

Community Divers is being built for players who want to share a persistent war,
run their own progression and choose their mods together. Each community can
operate its own campaign service, while players' machines run individual missions
using their own installed game assets.

The launcher uses a HELLDIVERS 2-inspired menu layout and is written in C++20
with Win32, Direct3D 11 and Dear ImGui. The portable core is
kept separate from Windows integration. It does not require a webview, Python or
.NET to run.

![Launcher overview](docs/images/launcher-overview.png)

## Launcher preview

[Download the Windows prerelease](https://github.com/Nnamllit1/community-divers/releases/tag/v0.1.0-preview.1)
and verify it with the accompanying SHA-256 file.

- Detect an existing game through Steam or choose its installation directory.
- Save community HTTPS endpoints and check their protocol/build advertisements.
- Inspect local mod manifests and save selections in named profiles.
- Recover settings without silently discarding a malformed original file.
- Export read-only client fingerprints for exact-build integration analysis.

Current source builds also check a signed preview update feed and stage verified
package downloads. Automatic installation is still pending; the first binary
preview predates this update functionality.

Run `out/windows-dev/CommunityDivers.exe` after building, or use the executable
in a portable package. Launcher data lives in `%LOCALAPPDATA%/CommunityDivers`.
A server connection does not establish game-client compatibility. Launch remains
unavailable until the isolated current-client adapter has been implemented.

See [service status](docs/service-contract.md) and
[mod manifests](docs/mod-manifests.md) for the current contracts.
[Client inspection](docs/client-integration.md) describes the build catalog and
unimplemented game-adapter gates.

## Development

Requirements: CMake 3.25+, a C++20 compiler, and the Windows SDK for the launcher.
Dependencies are downloaded from pinned releases at configure time.

On Windows, open a Visual Studio x64 developer terminal:

```console
cmake --preset windows-dev
cmake --build --preset windows-dev
ctest --preset windows-dev
```

`Build.cmd` finds the installed Visual Studio tools and runs the same steps.
Both dependencies are pinned by version and archive SHA-256.
See [validation](docs/testing.md) for core, native HTTP and offscreen widget checks.

To prepare a portable directory:

```console
cmake --install out/windows-dev --prefix dist/CommunityDivers
```

To produce a versioned ZIP and SHA-256 file:

```console
cpack --config out/windows-dev/CPackConfig.cmake -B dist
```

Packages include `build-info.json` with the exact source revision and release ID.
The [update documentation](docs/updates.md) covers signed preview checks and verified
downloads, plus planned CI publication, client/server release-set checks and rollback.

For the portable core on Linux:

```console
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev
```

See [PROJECT.md](PROJECT.md) for the editable roadmap and acceptance criteria,
[architecture](docs/architecture.md) for component boundaries, and
[CONTRIBUTING.md](CONTRIBUTING.md) to contribute.

## Scope

- Community-operated war campaigns and independent player progression.
- A native launcher, installation discovery, server selection and mod profiles.
- Isolated current-client integration and community multiplayer.
- HTTPS services and multiplayer transport without player-side port forwarding.

No game assets or proprietary game binaries are included. A genuine owned game
copy is required. Modified gameplay and private progression stay separate from
official services. Nothing in this repository establishes current-game patch
compatibility or official approval.

Community Divers is an independent fan project, not affiliated with or endorsed
by Arrowhead Game Studios or Sony Interactive Entertainment.

## License

Project code is [MIT licensed](LICENSE). Third-party components retain their own
licenses; see [THIRD_PARTY.md](THIRD_PARTY.md).
