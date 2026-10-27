# Component licensing

The Community Divers Service License 1.0 is effective for the designated service
component from the commit titled **Adopt Community Divers Service License 1.0**.
The launcher remains MIT. The standalone HellForge loader retains GPLv3 and its
mod exception. No service restrictions apply to that loader, its permissive SDK,
or independent mods and clients.

## Coverage

| Material | License |
| --- | --- |
| Original files under `src/service/`, `include/community/service.hpp`, `tests/service_tests.cpp`, and `examples/service/` | [Community Divers Service License 1.0](licenses/community-divers-service-1.0.md); identifier `LicenseRef-Community-Divers-Service-1.0` |
| Launcher, shared HTTP/data contracts and utilities, client preparation, patch notes, updates, installer, general build scripts and documentation | [MIT](../LICENSE), unless a file supplies its own notice |
| `integrations/hellforge-loader/` and fetched HellForge native loader | GPLv3 with HellForge's mod exception; SDK portions retain MIT-0 as specified upstream |
| Third-party dependencies and game files | Their own terms; see [third-party notices](../THIRD_PARTY.md) |

The service's C++ implementation API is covered. The public HTTP protocol and
shared launcher data types remain MIT: an independent service implementation does
not become covered by implementing the same protocol. Code derived from the
service remains covered even if placed in a different directory or process.

## Earlier MIT permissions remain available

Service material through commit
[`7b5c16d7`](https://github.com/Nnamllit1/community-divers/tree/7b5c16d77d12fee071309179e74e7251cfa03453)
was supplied under MIT. Those permissions and notices remain valid, including for
unchanged material carried forward. This transition cannot force recipients of
MIT-licensed material to abandon that grant or prevent an MIT fork of that code.
The new terms govern new covered material for which no separate MIT permission
has been given. Do not treat the adoption notice as retroactive exclusivity.

The existing service history contains project-authored changes in `8399a73`,
`ee4fd16` and `502cdeb`; no HellForge backend implementation is incorporated.
Dependencies keep their notices. Repository authorship history is not a substitute
for a contributor's assurance that they have permission to submit their work.

## Operator terms

Read the [full license](licenses/community-divers-service-1.0.md) before deploying
or distributing the service. Its funding rules permit unlimited genuine donations
and optional decorative ranks, titles and profile badges. Paid monthly revenue,
including shared access-cost contributions, may not exceed **twice actual monthly
eligible operating costs**. Donations neither consume nor enlarge that allowance.

At EUR 100 monthly costs, EUR 200 monthly sales leaves EUR 100. Saving four months
of that margin funds another four months at the same cost without sales. Lawfully
retained savings have no balance cap. The license defines cost allocation, refunds
of excess receipts and aggregate reporting. Paid pets and other cosmetics are not
authorized by version 1.0; they remain a future policy option.

Gambling, paid randomized rewards, gameplay advantages, donor priority and
privileged campaign influence are prohibited. Calling a purchase a donation does
not change its treatment.

Distributors provide the exact corresponding source and notices. Network operators
make that source available to users, including modifications and build instructions,
without exposing secrets or player data. See [self-hosting](self-hosting.md).

## Contributions and compatibility

Contributions use the owning component's terms; see [CONTRIBUTING](../CONTRIBUTING.md).
Do not import GPL-derived code into the restricted service. Communicating over an
interface does not by itself establish independence if code was copied or derived;
see [GNU's combined-work guidance](https://www.gnu.org/licenses/gpl-faq.en.html#MereAggregation).

The service is source-available, not OSI open source, because it restricts use and
monetization. See the [Open Source Definition](https://opensource.org/osd).
The custom text has not received independent legal review and is not an enforcement
guarantee. Adoption records the project's terms; it does not certify enforceability
or confer rights to third-party content.

Version 1.0 remains available for material received under it. A future version is a
separate grant, not a unilateral replacement for existing compliant permissions.
