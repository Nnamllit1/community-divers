# Third-party components

The build downloads pinned upstream source archives into its build directory.
No third-party application, game asset or runtime binary is committed here.

| Component | Version | License | Purpose |
| --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.93.0 | MIT | Native launcher UI and Win32/Direct3D 11 backends |
| [JSON for Modern C++](https://github.com/nlohmann/json) | 3.12.0 | MIT | Settings, mod manifests and service messages |
| [miniz](https://github.com/richgel999/miniz) | 3.1.2 | MIT | Bounded ZIP extraction for launcher updates |
| [SQLite](https://www.sqlite.org/) | 3.54.1 | Public domain | Campaign/profile database and online backups |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | 0.63.0 | MIT | Loopback HTTP community service |

Bahnschrift and Segoe UI are loaded from the Windows installation when available
and are not redistributed. The launcher falls back to Dear ImGui's bundled font.

Portable release packages must include this file and the dependency license files.

On Linux, the service links the system OpenSSL 3 Crypto library for secure random
credentials and SHA-256. Windows uses BCrypt. OpenSSL retains its upstream license.
