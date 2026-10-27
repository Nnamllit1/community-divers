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

Genuine voluntary donations are unlimited, including amounts retained by the
operator. They must not purchase or be expected in exchange for a benefit. Paid
purely decorative ranks, titles and profile badges are permitted within a separate
cap, along with disclosed shared hosting-cost contributions. Paid pets and other
cosmetics remain a possible future extension.

The paid-revenue allowance is **twice actual monthly eligible operating costs**.
This is a revenue cap, not permission for profit of twice the costs: EUR 100 in
hosting permits EUR 200 of monthly sales, leaving EUR 100 after hosting. Saving
that margin for four months builds EUR 400 to pay for another four months without
sales at the same cost. Donations are outside the cap and do not increase it.

The draft defines prorated infrastructure costs, calendar-month accounting and
refunds of over-cap sales. The retained lawful margin may be saved without a
balance cap or taken as operator income. Paying operator salaries or relabeling
purchases as donations must not enlarge the allowance. Aggregate reporting keeps
donation and sales income separate without publishing donor identities.

The intended prohibited activities are:

- real-money gambling, paid randomized rewards and cash-out schemes involving the
  covered service;
- selling gameplay advantages, progression, equipment, stats, priority access or
  privileged influence over the shared campaign;
- disguising purchases through donation tiers, third parties or bundled memberships
  to avoid benefit restrictions or the paid-revenue cap.

The [review draft](service-license-draft.md) includes worked examples and the
source-sharing conditions. These terms have not changed the effective license.

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

The draft permits decorative paid ranks and disclosed shared access-cost
contributions, but no paid gameplay tiers or donor priority. Genuine donations
remain uncapped. Operator compensation does not increase the cost base used to
calculate the paid-revenue cap.

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
3. Review the draft definitions, source obligations, donation/sales distinction,
   monthly paid-revenue cap, prohibited conduct and compliance/remedies.
4. Review enforceability and compatibility with qualified legal advice, then update
   file-level notices, contributor terms, installer/package notices and public docs
   together. Do not label the restricted service as open source.

A license provides legal terms, not a technical guarantee against abusive operators.
Community listing and branding rules can additionally govern the project's own
directory, but cannot replace the license or prevent independent implementations.
