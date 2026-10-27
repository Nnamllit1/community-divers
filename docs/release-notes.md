# Launcher preview

## Fixed

  * **Interrupted setup recovery.** A partially installed launcher now explains
    that setup must be run again, instead of trying to start with mixed or missing
    application files. Rerunning setup repairs the launcher while retaining mods
    and user data.
  * Unknown setup states are rejected instead of being treated as completed
    installations. Portable installations continue to work without a setup marker.

## For contributors

  * Installer checks now stop the real setup process midway through both a new
    installation and a reinstall, then verify normal repair, startup and removal.
    Fault-injection pauses are compiled only into separate QA artifacts.

## Current limits

The community game adapter and multiplayer are **still in development**. A verified
file copy and a connected community account do not enable game launch yet.
