# Contributing

Start with [PROJECT.md](PROJECT.md), the editable scope and acceptance criteria.
The first milestone is a native launcher with real installation and service state;
game integration and the campaign/multiplayer service follow it.

Build with a C++20 compiler and run the CTest suite before submitting a change.
Keep platform-independent behavior in `community_core`; Windows APIs belong in
the platform and launcher components.

Public documentation should help players and contributors understand the product,
contracts and evidence. Keep machine-specific paths, personal logs, credentials,
game assets and unfinished local experiments out of commits.

Changes to client integration need exact-build evidence and a clear distinction
between static analysis, synthetic tests and live gameplay. No unofficial session
should alter official progression or connect modified gameplay to official lobbies.
