# Current-client startup investigation

The current client adapter is unfinished. This page records the bootstrap
observed for Steam build **26088801** on 27 October 2026 and the limits of the
startup probe. It does not describe a playable community client.

## Bootstrap dependencies

Read-only PE inspection and a contained startup probe of the independently
prepared copy found:

- `bin/helldivers2.exe` imports `preloader.dll` and has TLS initialization callbacks.
- `data/game/game.dll` also imports the preloader and has TLS callbacks.
- The preloader starts `runtime.dll` as a **separate process** during initialization.
  The `.dll` filename must not be taken to mean it loads only into the game process.

The last point was observed in the contained process tree, where the child process
reported `runtime.dll` as its main module. The executable, preloader, runtime and
engine are separate integration dependencies. Lua support or unchanged networking
SDK binaries do not establish a working native bootstrap.

## Offline probe conditions

The development probe used a fresh Windows AppContainer with no network
capabilities, a private writable data directory and temporary read/execute access
to the independent copy. It did not patch game bytes, launch Steam or use the
protected launcher. The process tree stayed in an owned Windows job, was stopped
after a bounded observation period, and the temporary profile and access grants
were removed afterward.

Fixture processes verified the actual token, input-file read access, denied input
writes, denied outside-file reads/writes and private output writes. A control
process connected successfully to a loopback listener; the contained process
could not connect and the listener received no connection from it. The socket
attempt timed out rather than returning an immediate access-denied code. A child
fixture retained the same restrictions. These checks support the probe setup;
they are not full game-profile, Steam Cloud or community-network validation.

Microsoft documents the underlying [AppContainer launch and capability model](https://learn.microsoft.com/en-us/windows/win32/secauthz/implementing-an-appcontainer).

## Results and interpretation

A one-process job limit caused the preloader to report `STATUS_QUOTA_EXCEEDED`
(`0xC0000044`) before the runtime child could start. This was a restriction imposed
by the probe, not evidence that EAC had rejected the client. The status was read
from the preloader's preserved stack frame using standard debugger APIs.

With a four-process limit, a game process and runtime child were observed with
zero-capability AppContainer tokens. Both remained present during the 15-second
observation. The engine and networking SDK modules did not appear in the recorded
module lists, and no game profile files were written to the assigned data tree.
The job was then terminated by the probe; its timeout exit code is not a game
crash code. All 180 checked copy-file/directory access descriptors matched their
pre-probe values after cleanup.

This establishes a bootstrap process dependency, **not successful unprotected
startup**. AppContainer restrictions can themselves affect initialization. The
remaining runtime dependency or failure must be identified before selecting an
integration strategy; the observed wait does not establish EAC/Theia rejection,
prove that startup is impossible, or justify enabling the launch button.

Further work needs to identify the runtime child's initialization state, reach
engine startup, then verify separate profile/Cloud data and explicit routing to
the community backend. The [adapter acceptance gates](client-integration.md#adapter-acceptance-gates)
remain unchanged. Ordinary native stack walking may not unwind protected or
manually mapped frames reliably; unrecognized frames are not function evidence.
