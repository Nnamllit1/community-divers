# Launcher service contract, protocol 1

The launcher makes an unauthenticated GET to `<base URL>/v1/status`. The base URL
must use HTTPS; HTTP is accepted only on localhost, 127.0.0.1 or [::1]. Redirects
are not followed. The response must be HTTP 200 and no larger than 256 KiB.
TLS certificate validation is provided by Windows and is not disabled.

```json
{
  "protocol": 1,
  "id": "my-community",
  "name": "My Community",
  "game_builds": ["26088801"],
  "required_release_set": "campaign-preview-1",
  "required_launcher_release": "0.1.0-preview.1+12345678",
  "campaign": {"name": "Our First Campaign"}
}
```

`campaign`, `required_release_set` and `required_launcher_release` are optional
in protocol 1. All other fields are required. Unknown fields are ignored.
Build IDs are strings of decimal digits. A response advertises service information;
it cannot enable a missing local game adapter or prove game compatibility.
The example build ID is illustrative, not a compatibility declaration.

The status endpoint carries no player identity or authentication token. The
initial profile, campaign and mission-ledger contracts below are implemented;
lobby/gameplay contracts remain pending.

Release identifiers are nonempty ASCII strings of at most 128 letters, digits,
periods, hyphens, underscores or plus signs. The launcher displays the community's
required release set and rejects a mismatched launcher release as a readiness
prerequisite. A match does not enable the unfinished game adapter. The release-set
identifier is currently metadata: adapter/service component mapping and admission
enforcement remain pending. Community responses cannot select a new signing key
or make the launcher install arbitrary packages.

## Campaign snapshot, schema 1

A service advertising `campaign.read` in its optional `capabilities` array provides
`GET /v1/campaign`. The launcher fetches this after status, using the same base
endpoint, redirect policy and TLS validation. A campaign fetch error is shown
without reclassifying a valid status response as an invalid service.

```json
{
  "schema": 1,
  "community_id": "my-community",
  "name": "Our First Campaign",
  "revision": 2,
  "epoch": 1,
  "planets": [{"id":"example-planet","name":"Example planet","biome_id":"example-biome","owner_faction":"terminid","liberation":250000}],
  "templates": [{"id":"example-mission","name":"Example mission","reward_xp":100,"war_points":250}]
}
```

The community ID must match status. Integers are nonnegative, epoch is positive,
liberation/reward values are at most one million, identifiers are bounded, and
planet/template IDs are unique within their respective collections. The launcher
limits snapshots to 1 MiB and 1024 items per collection. Revision changes with
operator content edits and finalized mission results. Initial campaigns are empty.
The payload is persisted service state, not a proof of native game compatibility.

## Authenticated service operations

Use `Authorization: Bearer TOKEN`. No cookies, query-string credentials or official
account identifiers are used. Mutation bodies must be JSON, at most 64 KiB and no more than 32 levels deep.
Errors return a JSON `error` code and `message`; secrets and SQL details are omitted.

| Method/path | Credential | Behavior |
| --- | --- | --- |
| GET `/healthz` | None | Liveness and protocol |
| GET `/v1/status` | None | Community/build/version metadata |
| GET `/v1/campaign` | None | Persisted public campaign |
| POST `/v1/admin/players` | Operator | Issue a community profile and one returned bearer credential; body `{ "name": "Player" }` |
| POST `/v1/admin/planets` | Operator | Upsert `id`, `name`, `biome_id`, `owner_faction`, `liberation` |
| POST `/v1/admin/templates` | Operator | Upsert `id`, `name`, `reward_xp`, `war_points` |
| GET `/v1/me` | Player | Own ID, name, XP, successful mission count and loadout |
| PUT `/v1/me/loadout` | Player | Store an arbitrary object of at most 8 KiB/32 top-level entries on that profile |
| POST `/v1/missions` | Player | Create a solo ledger lease with configured `planet_id` and `template_id` |
| POST `/v1/missions/{id}/result` | Lease owner | Finalize `{ "outcome": "success" }`, `failure` or `aborted` |

Mission admission compares `X-Community-Launcher` and
`X-Community-Release-Set` with configured requirements, if present. These are
compatibility declarations, not attestation of an unmodified client. A mismatch
returns 409. Leases currently expire after 24 hours and belong to one community
profile. Squad membership and host result delegation await lobby integration.

Rewards are snapshotted on lease creation. Client-supplied reward fields are
rejected. Result application, profile progress, campaign contribution and receipt
are committed in one SQLite transaction. Repeating the same result returns the
original receipt with `replayed: true`; changing the finalized outcome returns
409. Replays do not update progression or campaign revision. A receipt's profile
is the original result snapshot; GET `/v1/me` provides the latest profile.

Live mission execution, contribution validation against game callbacks, campaign
scheduling, operations, voting and realtime gameplay transport are not implemented.
See [self-hosting](self-hosting.md) for initialization, credentials and recovery.

The native launcher imports an operator-issued player credential and checks the
community/player identities before remembering it. See [player profiles](player-profiles.md)
for the sign-in, storage and local sign-out contract.

## Service source notice

The reference service adds a `source` object to `/v1/status` and returns that same
object at unauthenticated `GET /v1/source`: `license`, `url`, `source_commit` and
`source_dirty`. These are informational fields, not authentication or a remote
installation request. The homepage provides the source link for users in a browser.
Existing protocol-1 clients ignore the additive fields. A missing source URL on a
dirty development build is not a claim that upstream matches its modifications.
