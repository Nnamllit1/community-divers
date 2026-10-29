# Launcher preview

## Client inspection

  * **Startup dependencies are now checked.** Client inspection fingerprints the
    preloader, runtime and two stock engine plugins alongside the four core
    client modules.
  * Changed, missing or unreadable startup files prevent an observed-build match
    and appear in the inspection report. Existing prepared-copy receipts keep
    their format and continue to work.

## Native Windows setup

  * **A new C++ installer and uninstaller** match the launcher's dark panels,
    condensed headings and yellow controls. Choose an installation folder, follow
    progress and open the launcher from the completion screen.
  * Setup remains **per user**, with a Start menu shortcut and Windows app-list
    entry. The full launcher package is embedded and verified before installation.
  * Rerun setup to repair an interrupted installation. Uninstall preserves profiles,
    mods, campaigns, game copies and modified launcher files.
  * Portable downloads and the launcher's signed update system continue to work.
    Existing installations do not need the installer to receive launcher updates.

## Current limits

The launcher remains a **preview**. Current-game startup and community multiplayer
are still in development. Client inspection identifies modules; it does not
establish game compatibility or enable launch. Native setup keyboard navigation
and monitor DPI transitions still need interactive verification.
