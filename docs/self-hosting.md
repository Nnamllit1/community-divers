# Self-hosting the community service

`CommunityDiversService` is a C++20 HTTP service with SQLite persistence. It owns
its community profiles, loadouts, campaign configuration and mission ledger.
The launcher can display its persisted campaign through `/v1/campaign`.

This is the first service implementation. It does not make the current game
connect to the community, launch missions or provide gameplay transport. The
native game adapter, lobby integration, schedules, voting and the server update
agent remain in development. API mission tests are not game mission validation.

## Build and install

Build on Windows with `Build.cmd`, or on Linux with the `linux-dev` CMake presets.
Linux requires OpenSSL 3 development files in addition to CMake, a C++20 compiler
and Ninja. SQLite 3.54.1 and cpp-httplib 0.63.0 are downloaded as hash-pinned source.
The application does not require Python, Node or a separate database service.

```console
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev
cmake --install out/linux-dev --prefix dist/service --component Server
```

On Windows, use `out/windows-dev` in the install command. The service executable
is built alongside the launcher, but its explicit Server install component is
excluded from launcher ZIPs. CI provides separate Windows/Linux service artifacts.
Run service updates only after stopping the old service; automatic drain and
service rollout are not implemented yet.

## Initialize a community

Copy and edit `examples/service/community.json`. The community ID identifies the
database and must remain stable. `name` and `campaign` initialize its display
metadata. The initial campaign has no invented planets or mission templates.
The default advertised game-build list is empty because current-game integration
is not verified. Adding a build number advertises operator configuration only.

```console
CommunityDiversService init community-data --config examples/service/community.json
CommunityDiversService serve community-data
```

`init` requires a new data directory. It creates owner-only data/credential files,
`config.json`, `operator.token` and `state.sqlite`. Runtime SQLite WAL files belong
with that database. Keep the data directory private and outside the application
install folder. Initialization, backups, restores and credential exports refuse
to overwrite existing outputs.

The HTTP listener accepts loopback addresses only. For local testing, add
`http://127.0.0.1:8080` in the launcher's Communities view, press Check selected,
then open Campaign. Refresh campaign reloads the persisted snapshot.

## Publish through HTTPS

Place an HTTPS reverse proxy on the same host. For example, with a domain you
control and a configured Caddy installation:

```caddyfile
divers.example.com {
    reverse_proxy 127.0.0.1:8080
}
```

Players use `https://divers.example.com` as their launcher endpoint. The operator
must arrange DNS, ingress and TLS for that host. No player-side port forwarding
is required for these outbound campaign requests. Gameplay relays are a separate,
unimplemented transport requirement. Local tests do not verify a public HTTPS
deployment. See [Caddy's reverse proxy documentation](https://caddyserver.com/docs/caddyfile/directives/reverse_proxy)
for deployment options.

## Profiles and operator controls

The operator token authorizes `/v1/admin/*`; player tokens authorize only their
own profile and mission ledger entries. Do not publish the operator token or
include it in launcher packages. Tokens are random bearer credentials; player
token hashes are stored in SQLite. These are community identities, not official
Steam/Arrowhead login credentials or proof of game ownership.

Create a player credential file without printing its token:

```console
CommunityDiversService player community-data --name "Player name" --output player.credential.json
```

Deliver that file privately to its owner. Launcher profile login and self-service
registration/revocation are not implemented yet; the current profile API is for
adapter/service development. Protect credentials as passwords.

Operators can configure arbitrary bounded content identifiers through the JSON
API. Identifiers are community configuration until a supported game adapter maps
them to actual installed content. These example IDs are API examples:

```sh
operator_token="$(cat community-data/operator.token)"
curl -H "Authorization: Bearer ${operator_token}" -H 'Content-Type: application/json' \
  -d '{"id":"example-planet","name":"Example planet","biome_id":"example-biome","owner_faction":"terminid","liberation":0}' \
  http://127.0.0.1:8080/v1/admin/planets
curl -H "Authorization: Bearer ${operator_token}" -H 'Content-Type: application/json' \
  -d '{"id":"example-mission","name":"Example mission","reward_xp":100,"war_points":250}' \
  http://127.0.0.1:8080/v1/admin/templates
unset operator_token
```

Liberation uses integer millionths: `1000000` is 100%. Successful mission results
apply the snapshotted rule, clamp contribution at the planet's remaining progress
and switch a newly liberated planet to `human` ownership. Failed/aborted results
award no XP or liberation. Only successful results increment `missions_completed`.
Mission completion currently trusts authenticated community reports; there is no
claim of anti-cheat or verified game execution.

## Back up and restore

Use the native backup command while the service is running or stopped:

```console
CommunityDiversService backup community-data --output campaign-backup
CommunityDiversService restore campaign-backup --output restored-community-data
```

Backup uses SQLite's online backup API, checks the database, and records SHA-256
checksums for the snapshot, configuration and operator credential. A backup
contains the operator credential and should remain private. Restore verifies
checksums and the community identity, requires a new destination, and rejects
unsupported schemas. Stop the old service before starting the restored one with
its configured port. The existing data directory is never overwritten by restore.

The initial schema migrates an empty initialized database to version 1. Future
versions are rejected; no downgrade or destructive recovery is attempted.
The service refuses unrelated databases and a configuration with another
community ID. [SQLite's online backup API](https://www.sqlite.org/backup.html)
provides the underlying consistent database snapshot mechanism.
