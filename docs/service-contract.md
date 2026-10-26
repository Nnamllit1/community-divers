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

This initial endpoint carries no player identity, authentication token or
progression. Account, campaign and lobby contracts follow in later milestones.

Release identifiers are nonempty ASCII strings of at most 128 letters, digits,
periods, hyphens, underscores or plus signs. The launcher displays the community's
required release set and rejects a mismatched launcher release as a readiness
prerequisite. A match does not enable the unfinished game adapter. The release-set
identifier is currently metadata: adapter/service component mapping and admission
enforcement remain pending. Community responses cannot select a new signing key
or make the launcher install arbitrary packages.
