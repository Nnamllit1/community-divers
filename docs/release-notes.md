# Launcher preview

## Clearer versions

  * **Numbered previews** replace commit hashes in release titles and download
    names: for example, `0.1.0-preview.45`. About displays the same version.
    The commit hash remains available in build information for diagnostics.
  * A larger preview number means a newer main revision. Numbers can skip when
    source changes do not produce a release.
  * Late CI builds are checked before publication so an older build cannot be
    newly published after a newer preview. Existing updater installations accept
    the new versions; older download links continue to work.

## Current limits

The launcher remains a **preview**. Current-game startup and community multiplayer
are still in development. This release changes versioning and publication, not
game compatibility.
