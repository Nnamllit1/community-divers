# Automatic updates and release compatibility

Automatic updating is part of the project scope. Current source builds check the
signed preview feed at startup and hourly, with an opt-out setting under About.
They can download and verify a package into launcher data. Portable installations
also provide Install and restart: the helper replaces owned launcher files after
the old process exits, checks the replacement's startup and rolls back failures.
The CI publisher promotes successful component builds to the signed preview feed.
`Update.cmd` runs a noninteractive native update check/download/install handoff.
Automatic in-app installation scheduling, server admission checks and the server
update agent remain unimplemented.

Use a current preview package for update support. The original `v0.1.0-preview.1`
predates the updater and must be replaced manually.

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
promotion remain pending. The accepted channel envelope is persisted and reauthenticated on subsequent
checks. An older observed revision or a changed package for the same revision is
rejected. A corrupt saved channel state is reported rather than silently reset.
Trust-root rotation across multiple publisher keys remains pending; current
installations use one compiled publisher key.

Native implementation references: [Windows signature verification](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptverifysignature)
and [WinHTTP response headers](https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpqueryheaders).

## Launcher installation and recovery

Install and restart is available in portable installations containing
`install-inventory.json`. To make one from source:

```console
cmake --install out/windows-dev --prefix dist/CommunityDivers
```

Use a writable application directory separate from launcher data and the game.
The inventory lists only files installed by the project; it does not adopt
unrelated files already in that directory. Packages include the inventory,
`build-info.json` and launcher update protocol 1. The original binary preview
lacks that protocol and cannot be an automatic installation target.

The helper is copied into a unique job directory outside the application. It
independently checks the publisher signature, the observed channel revision,
package size/hash, bounded ZIP contents, each file's inventory hash and the
packaged build identity. Paths with traversal, Windows aliases, duplicate names,
links, unexpected top-level content or excessive expanded sizes are rejected.
No downloaded package code runs during this preparation.

Backups and a durable journal are completed before the helper signals that the
launcher may exit. The parent is identified by PID, creation time and executable
path. A timed-out preparation is stopped before reporting failure. A per-install
file lock excludes another launcher during replacement and is inherited by the
replacement process. The helper replaces individual managed files atomically,
removes obsolete inventory entries and preserves unmanaged files.

The replacement acknowledges startup after rendering and checking its compiled
source identity against build metadata. A process exit, missing acknowledgement
or early crash triggers restoration of the verified backup and restarts the old
launcher. The journal and recovery pointer allow the next launch to restore an
update interrupted by helper/process failure. Backups remain in launcher data
under `updates/jobs`; automatic history cleanup is not implemented yet.

The native process tests cover successful restart, early failure, a hung startup,
unsafe ZIP rejection, independent helper signature rejection and recovery after
an updater crash midway through replacement. An optional real-launcher smoke
check additionally exercises Direct3D rendering and the startup acknowledgement;
these results do not establish game-adapter or server update support.

Maintainers can inspect a signed bundle without installing or executing it:

```console
CommunityDiversUpdate.exe --verify-package package.zip signed-feed.json extracted-package
```

## Preview publishing pipeline

`publish-preview` runs only on main after Windows and Linux checks pass. It
consumes the Windows job's package, checks its source identity/checksum, signs
through the native updater using an environment secret and verifies extraction
before creating a draft release. The source tag identifies the full build commit;
ZIP names include the version and short commit. Draft retries may replace draft
assets; published assets are checked and never overwritten.

The release is published before `updates/preview.json` is promoted through a
compare-and-swap Contents API update on main. A competing/newer channel revision
cannot be replaced by an older one. Channel-only and documentation-only pushes
do not promote releases. The workflow uses its scoped GITHUB_TOKEN for repository
writes, preventing channel commits from recursively starting another workflow.
Stable-tag publication and combined service/adapter release sets remain planned.

For script-based launcher updates, run `Update.cmd`, or:

```console
CommunityDivers.exe --update --data-dir custom-launcher-data
```

The command uses the same pinned key, replay protection, ZIP validation and
restart helper as About. It exits successfully without replacement when the
installed release is current or newer, and returns an error without applying an
update if the build ordering or compatibility cannot be verified. Errors are
recorded in launcher data. A successful upgrade opens the replacement launcher.
The original launcher must be closed before running the command; its installation
lock prevents concurrent instances. This command updates launcher files only.
