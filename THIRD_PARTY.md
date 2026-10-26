# Third-party components

The build downloads pinned upstream source archives into its build directory.
No third-party application, game asset or runtime binary is committed here.

| Component | Version | License | Purpose |
| --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.93.0 | MIT | Native launcher UI and Win32/Direct3D 11 backends |
| [JSON for Modern C++](https://github.com/nlohmann/json) | 3.12.0 | MIT | Settings, mod manifests and service messages |

Segoe UI is loaded from the Windows installation when available and is not
redistributed. The launcher falls back to Dear ImGui's bundled font.

Portable release packages must include this file and the dependency license files.
