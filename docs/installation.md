# Installing the launcher

Download a Windows x64 preview from [Releases](https://github.com/Nnamllit1/community-divers/releases).
Each release includes a Setup executable, a portable ZIP, SHA-256 files and its
release notes. Game files are not included. Community game launch remains under development.

## Windows setup

Run `CommunityDiversSetup-<release>-windows-x64.exe`. Setup installs for the current
Windows account without elevation. The default folder is
`%LOCALAPPDATA%/Programs/CommunityDivers`. Choose an empty folder or an installation
created by this setup; close the launcher and any pending updater first.

The native C++ setup uses the launcher's dark panels and yellow controls. Choose
the destination, install, then launch directly from the completion screen. The
uninstaller uses the same interface and shows the location before removal.

Setup adds a Start Menu shortcut and a Windows installed-apps entry. It does not
install a service, create a desktop shortcut, change Steam or modify your game.
Launcher settings and profiles remain in `%LOCALAPPDATA%/CommunityDivers`.
The launcher and setup are not currently Authenticode-signed; the provided SHA-256
files identify the release artifacts, and in-app updates use a separate pinned
publisher signature.

If file installation is interrupted, close the launcher and rerun the same setup.
The ownership marker permits repair of an incomplete installation. Until repair
finishes, starting the launcher explains that setup is incomplete and exits; it
does not try to update or run partially installed application files. A running
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

The build produces `CommunityDiversSetup.exe`, a native setup stub. The packaging
command verifies the portable ZIP's complete inventory and exact source identity,
then embeds it in a self-contained executable with its SHA-256 file. No separate
installer compiler is downloaded. Use `-BuildDirectory` for a different build
directory; the stub and ZIP must be built from the same source revision.

`-TestIdentity` creates a separate QA registration/shortcut. `-AllowDirty` and
`-TestInterrupt` are available only with that QA identity. Production setup requires
clean sources. Automated checks exercise the packaged executable, interrupted
install/repair, corrupted payload refusal, uninstall preservation and rendered
setup screens. Desktop keyboard input and monitor DPI transitions still need
interactive verification.

For unattended installation, use `--silent --install-dir "C:\path\CommunityDivers"`.
The installed `Uninstall.exe --silent` hands removal to a separate temporary
process so Windows can release its executable. The temporary helper may remain
in the Windows temporary folder until that folder is cleaned. Developer checks
can invoke an external copy with `--silent --uninstall-dir "C:\path\CommunityDivers"`
and wait directly for the removal result.
