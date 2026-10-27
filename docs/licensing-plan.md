# Component licensing plan

This is a policy design, not a replacement license grant. The repository's current
[LICENSE](../LICENSE) remains MIT until a reviewed transition is adopted. Already
published MIT copies retain their existing permissions. Third-party components
retain their own licenses.

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
fees. This number is a draft, not an agreed term. Eligible costs, reserve treatment,
operator compensation and accounting periods need clear definitions; "small profit"
on its own does not give operators a predictable rule.

The license should distinguish actual shared hosting costs from fees for cosmetic
items or gameplay benefits. Donation records must not require disclosure of donor
identities to the public. Any funding transparency requirement should use aggregate
figures and preserve personal information.

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
