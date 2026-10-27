# Launcher preview

## Fixed

  * **Prepared-copy build identification.** Inspect client files now recognizes a
    prepared game copy without Steam's external manifest. It checks the receipt
    against all four current binary fingerprints and displays the copied build.
    Stale or conflicting receipts produce an actionable error.

## For contributors

  * Identity reports distinguish Steam metadata from preparation receipts and
    include metadata errors. Inspection hashes the same file handles held open
    throughout the check.
  * Prepared-copy checks cover receipt validation, changed binaries, metadata
    conflicts and the build display in the launcher.

## Current limits

Receipt inspection checks the four diagnostic binaries. Use the copy verifier to
check every prepared asset. The community game adapter and multiplayer are
**still in development**; identifying a prepared copy does not enable game launch.
