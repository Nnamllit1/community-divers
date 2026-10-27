# Launcher preview

## New

  * **Patch notes in the launcher.** Read formatted release notes with headings,
    bold text and optional images from About. View an update's notes before
    installing it, or read the installed build's notes offline.
  * **Windows installer and uninstaller.** Install for your Windows account with
    a Start Menu shortcut and an installed-apps entry. Uninstalling preserves
    profiles, mods, game copies and custom files.

## For contributors

  * Application, patch notes and mod discovery now have separate source modules,
    public interfaces and component tests. Backend, updates, setup and client
    preparation have dedicated folders too.
  * Releases publish the same notes in the launcher and on GitHub, with versioned
    setup and portable downloads.

## Fixed

  * Installer builds retry official compiler mirrors if a download returns an
    HTML page or incomplete archive, while keeping the compiler checksum pinned.

## Current limits

The community game adapter and multiplayer are **still in development**. A verified
file copy and a connected community account do not enable game launch yet.

![Launcher overview](images/launcher-overview.png)
