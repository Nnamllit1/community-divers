# Installing the launcher

Download a Windows x64 preview from [Releases](https://github.com/Nnamllit1/community-divers/releases).
Each release includes a Setup executable, a portable ZIP, SHA-256 files and its
release notes. Game files are not included. Community game launch remains under development.

## Windows setup

Run `CommunityDiversSetup-<release>-windows-x64.exe`. Setup installs for the current
Windows account without elevation. The default folder is
`%LOCALAPPDATA%/Programs/CommunityDivers`. Choose an empty folder or an installation
created by this setup; close the launcher and any pending updater first.

Setup adds a Start Menu shortcut and a Windows installed-apps entry. It does not
install a service, create a desktop shortcut, change Steam or modify your game.
Launcher settings and profiles remain in `%LOCALAPPDATA%/CommunityDivers`.
The launcher and setup are not currently Authenticode-signed; the provided SHA-256
files identify the release artifacts, and in-app updates use a separate pinned
publisher signature.

If file installation is interrupted, close the launcher and rerun the same setup.
The ownership marker permits repair of an incomplete installation. A running
launcher or unresolved update transaction prevents replacement.

## Portable use and updates

Extract the ZIP into a new folder and run `CommunityDivers.exe`. Portable packages
do not create shortcuts or an app-list entry. Their data uses the same default
user-data folder, or a separate path supplied through `--data-dir`.

About offers signed update checks, **Read update notes**, and **Install and restart**.
**What's new in this build** reads the packaged notes even when offline. Setup
installations use the same ZIP-based updater as portable packages; it refreshes
the app-list version after a successful update. No installer is executed by the
in-app updater. See [updates](updates.md) for rollback and recovery behavior.

## Uninstall

For a setup installation, use Windows' installed-apps list or `Uninstall.exe`.
Close the launcher first. The uninstaller removes inventoried files only when
their size and hash still match, then removes its own shortcut and registration.
Modified resources and unknown files remain, so the install folder may remain too.

Profiles, settings, saved sign-ins, downloaded mods, game copies and campaign
databases are preserved. Remove those separately only when you intend to discard
them. Sign out from a community before uninstalling if you also want its remembered
Windows credential removed. A portable folder can be removed manually after
preserving any custom files you put inside it.

## Building setup

Produce a clean portable package, then run:

```powershell
cpack --config out/windows-dev/CPackConfig.cmake -B dist
./tools/build-setup.ps1 -PortableZip dist/CommunityDivers-<version>-windows-x64.zip
```

The builder downloads the pinned NSIS archive, checks its digest, verifies the
portable inventory and emits a setup executable plus its SHA-256 file.
`-AllowDirty` is for local development packages. `-TestIdentity` creates a separate
QA registration/shortcut for automated install/uninstall checks.
