# Contributing

Start with [PROJECT.md](PROJECT.md), the editable scope and acceptance criteria.
The first milestone is a native launcher with real installation and service state;
game integration and the campaign/multiplayer service follow it.

Build with a C++20 compiler and run the CTest suite before submitting a change.
Keep changes in their [owning module](docs/architecture.md#source-modules).
The application, patch-note renderer and mod loader have separate targets and
public interfaces. Portable shared contracts belong in `community_core`; Windows
APIs belong in the platform, maintenance and application components.

Public documentation should help players and contributors understand the product,
contracts and evidence. Keep machine-specific paths, personal logs, credentials,
game assets and unfinished local experiments out of commits.

Changes to client integration need exact-build evidence and a clear distinction
between static analysis, synthetic tests and live gameplay. No unofficial session
should alter official progression or connect modified gameplay to official lobbies.

For every component release, update `docs/release-notes.md` with the changes users
will notice. Use headings, bold emphasis and concise lists. Optional PNG/JPEG images
use paths under `docs/images/`, referenced as `images/name.png`. The native viewer
and GitHub release use the same file; publication rejects a component push that
does not update its release notes. Avoid private development history or test logs.
