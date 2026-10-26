# Automatic updates and release compatibility

Automatic updating is part of the project scope. Current source builds check the
signed preview feed at startup and hourly, with an opt-out setting under About.
They can download and verify a package into launcher data without changing the
installed launcher. Automatic installation, CI feed publication, server admission
checks and the server update agent remain unimplemented.

The first downloadable preview predates the update checker. Build current source
to use this functionality until a newer binary release is published.

## From a push to an update

1. A push to `main` triggers Windows/Linux build and test jobs.
2. Successful CI produces an immutable, versioned launcher/client bundle and
   server package, each identified by its source commit and SHA-256 digest.
3. Publication updates the selected channel manifest only after all required
   packages are available. Development component changes use a preview channel; stable
   releases use version tags. Documentation-only pushes do not promote a new
   channel release. Failed builds leave the channel's current release.
4. Launchers and server update agents check the HTTPS channel feed. Community
   operators choose whether their server follows a channel or stays pinned.
5. A community advertises its required release set. Players update to that set
   before joining; the service checks it again at lobby admission.

An update channel points to immutable artifacts rather than an archive whose
contents change at the same URL. Signed manifests establish the publisher;
artifact hashes validate the downloaded bytes. Publishing credentials stay in
CI secrets, outside source control and client packages.

## Release sets

A release set pairs components that work together. Server and client component
version numbers can differ; their shared release-set identity must match.

| Field | Purpose |
| --- | --- |
| `release_set` | Immutable identifier pinned by a community and its lobbies |
| `source_commit` | Exact published source revision |
| `launcher_version` | Launcher component version |
| `adapter_version` | Native game integration component version |
| `service_version` | Campaign/lobby backend component version |
| `service_protocol` | Request and callback contract version |
| `game_identity` | Required Steam build and client-file fingerprint profile |

The service status/handshake will advertise `required_release_set`. The launcher
compares its installed components and game identity before requesting admission.
The lobby checks the same set, preventing a stale launcher from joining anyway.
Existing missions keep their established release set until they end.

Game updates come from the player's owned installation. Community update bundles
contain project components; isolated game copies are refreshed and verified from
that local source. An updater cannot make an unsupported game build compatible.

## Client installation

- Check at startup, periodically while idle, and before joining.
- Show download/apply state and the community's required version.
- Download to a staging directory, verify the manifest and package, then prepare
  the new version beside the active one.
- Apply launcher changes through a small helper after the launcher exits. Apply
  adapter changes after the game session ends. Identify the running instance by
  a fresh process/session descriptor rather than PID alone.
- Switch atomically, keep the previous version until startup succeeds, and roll
  back failed starts. Preserve settings, profiles and local mods separately.

An offline player updates when reconnecting. Version consistency is enforced
at admission rather than assuming all installations are always online.

## Server updates

The operator-controlled agent checks the same release feed over outbound HTTPS.
It stages the selected server version, stops admitting new missions, waits for
active missions to finish, backs up persistent data, runs required migrations,
starts the new service and checks health before reopening admission. A failed
health check restores the prior service and the matching data backup when a
migration requires it. Operators can pin, defer or roll back a release.

This preserves the no-player-port-forwarding design. Update downloads use outbound
HTTPS; gameplay transport and campaign service hosting remain separate concerns.

## Current channel format and tools

`updates/preview.json` is an ECDSA-P256/SHA-256 signed envelope. Its `payload` is
a UTF-8 JSON string; the signature covers those exact bytes, including whitespace.
The signature is the 64-byte IEEE P1363 representation encoded as lowercase hex.
The public Windows CNG ECC blob in `updates/publisher-key.txt` is compiled into
the launcher; a community response cannot replace that trust root. A changed
payload, signature or key fails before any release is offered.

The payload contains schema 1, preview channel, Windows x64 platform, release ID,
full source commit, first-parent source revision, service protocol, package name,
release URL, byte count and SHA-256. Revision ordering applies to this repository's
main history; unknown/divergent ordering is reported rather than inferred. Game
adapter/service release-set enforcement is still a separate milestone.

Manifest responses are limited to 64 KiB and packages to 512 MiB. The production
transport requires HTTPS with normal Windows certificate validation, disables
cookies and authentication, and follows at most four redirects within an explicit
GitHub release/CDN host list. Downloads stream to a unique `.partial` file; size
and SHA-256 must match before it becomes a digest-named cached ZIP. A cache hit
is rehashed before reuse. Failed transfers preserve prior packages. Staging does
not extract or execute package contents. An abrupt process exit may leave an
unused partial file; completed cache files remain separately identifiable.

After building on Windows:

```console
out/windows-dev/CommunityDiversUpdate.exe --check
out/windows-dev/CommunityDiversUpdate.exe --download staging
```

The CLI uses the same pinned key, transport and staging code as the launcher.
About also provides Check for updates and Download verified package. The latter
stays unavailable when the published release matches or predates a known local
build. Settings, mod profiles and the game directory are untouched by staging.

Maintainers can generate a key pair and sign a payload using the native tool:

```console
CommunityDiversUpdate.exe --generate-key .local/keys/update-publisher
CommunityDiversUpdate.exe --sign payload.json .local/keys/update-publisher.private signed-feed.json
```

Generation and signing refuse existing output paths. Store the private key in
ignored local storage and an appropriate secret backup; never publish it. The
initial channel was signed manually. CI secret provisioning and automatic
promotion remain pending. Trust-root rotation and persisted replay protection
must be completed before automatic installation is enabled; the current checker
never installs or rolls back an application on its own.

Native implementation references: [Windows signature verification](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptverifysignature)
and [WinHTTP response headers](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpqueryheaders).
