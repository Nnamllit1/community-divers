# Component licensing plan

This is a policy design, not a replacement license grant. The repository's current
[LICENSE](../LICENSE) remains MIT for the existing launcher/service. The separate
[loader build integration](../integrations/hellforge-loader/README.md) now carries
GPLv3 and the mod exception; it is not part of the launcher distribution. Already
published MIT copies retain their existing permissions. Third-party components
retain their own licenses. The [review draft](service-license-draft.md) makes the
proposed funding and source-sharing rules concrete; it is not in force.

## Intended split

- General launcher, patch-note components and reusable shared utilities: retain MIT.
- Reused HellForge loader and modifications: GPLv3 with HellForge's mod exception,
  including corresponding-source and notice obligations. The permissive SDK may
  be used under its MIT-0 grant. See the [component review](hellforge-evaluation.md).
- Independently written campaign/backend services: a reciprocal source-available
  license with defined monetization restrictions. Do not apply those restrictions
  to HellForge-derived code or independent mods through a blanket repository notice.

Reciprocity should require distributors to provide corresponding source and
operators of modified network services to offer that source to their users,
including changes and the material needed to build the covered service. Independent
mods and unrelated programs should retain their own licenses. The precise covered
files, interfaces, notices and packaging rules must be documented before adoption.

## Funding policy

The intended permitted funding is voluntary donations and reasonable hosting-cost
recovery. Donations must not buy gameplay advantages or other paid entitlements.
Operators may retain a modest surplus; the policy is not intended to require every
small community to balance to exactly zero. Paid cosmetics are a possible future
policy change and are not part of the present permission being designed.

The intended prohibited activities are:

- real-money gambling, paid randomized rewards and cash-out schemes involving the
  covered service;
- selling gameplay advantages, progression, unlocks, equipment, stats or privileged
  influence over the shared campaign;
- routing equivalent purchases through donation tiers, third parties or bundled
  memberships to avoid these conditions.

A final license needs an objective surplus rule. A proposal for review is an annual
operator-retained surplus of at most **10% of documented eligible operating costs**,
with excess funding retained for future service costs, returned, or used to reduce
fees. This number is a draft, not an agreed term. The draft defines eligible
infrastructure costs, documented future-cost reserves,
calendar-year accounting and operator compensation within the surplus allowance.
It includes worked examples and aggregate reporting without public donor identities.
These details, including the cap, remain review proposals until adopted.

The license should distinguish actual shared hosting costs from fees for cosmetic
items or gameplay benefits. Donation records must not require disclosure of donor
identities to the public. Any funding transparency requirement should use aggregate
figures and preserve personal information.

## Proposed coverage and adoption boundary

No file changes license merely because it appears in this table. An adopted
release needs explicit notices and an exact revision identifying the transition.
Previously granted MIT permissions and notices must be preserved.

| Material | Proposed treatment |
| --- | --- |
| Original service implementation in `src/service/`, service implementation API in `include/community/service.hpp`, dedicated `tests/service_tests.cpp`, and service examples | Candidate service-license scope for new copyrightable material; inventory rights and preserve existing MIT grants before adoption. |
| Launcher, shared data contracts, patch-note renderer, update/installer code and general documentation | Remain MIT; no service restrictions on independent clients or implementations. |
| `integrations/hellforge-loader/` and fetched HellForge native loader | Existing GPLv3 and mod exception; never add this draft's commercial restrictions. |
| Third-party libraries, SDK and game files | Their own terms; no relicensing through this proposal. |

The source-sharing requirement covers modifications derived from the service code,
not every program on the same network. Separate processes and HTTP interfaces do
not by themselves establish that code is independent; copied or derived GPL code
cannot be moved into the restricted service by changing its deployment layout.
See [GNU's explanation of combined works and communication](https://www.gnu.org/licenses/gpl-faq.en.html#MereAggregation).

The draft permits disclosed shared access-cost contributions but no paid gameplay
tiers, donor priority or paid cosmetics. Operator compensation counts toward the
surplus cap rather than increasing the cost base used to calculate that cap.

Before adoption, review contributor rights and the final wording with qualified
legal advice. The draft is a proposal, not a legal opinion or an enforcement
guarantee. Do not change package licenses, contributor terms or active source
notices until the transition is actually adopted.

## Transition and compatibility

This design combines source reciprocity with use restrictions, so the service
would be source-available rather than OSI open source. Plain GPL/AGPL copyleft does
not prohibit commercial operation or gambling. See the
[Open Source Definition](https://opensource.org/osd) and
[GNU's network-source explanation](https://www.gnu.org/licenses/gpl-faq.en.html#UnreleasedModsAGPL).

Before changing effective terms:

1. Identify covered original code and any contributor rights; preserve legacy MIT
   and third-party notices. New restrictions cannot revoke existing grants.
2. Review the separate service/loader interfaces and avoid importing GPL code into
   a component intended to carry incompatible restrictions.
3. Finalize the surplus rule and draft the actual license text, including definitions,
   source obligations, permitted funding, prohibited conduct and compliance/remedies.
4. Review enforceability and compatibility with qualified legal advice, then update
   file-level notices, contributor terms, installer/package notices and public docs
   together. Do not label the restricted service as open source.

A license provides legal terms, not a technical guarantee against abusive operators.
Community listing and branding rules can additionally govern the project's own
directory, but cannot replace the license or prevent independent implementations.
