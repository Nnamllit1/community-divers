# Driver-free current-client integration

Assessment date: 28 October 2026. Examined client: Steam build **26088801**.
No working protection-removal patch or current-game community startup is available.

## Feasibility

A version-specific transformation of a player's independently prepared game copy
is a plausible deployment model. The size of a binary delta does not establish
whether it can remove a runtime dependency. First, the client must initialize
correctly without relying on development-machine state. Only then can that result
be packaged and tested on another machine.

Community Divers requires a player-side solution without custom kernel drivers,
test-signing or disabled driver-signature enforcement. The official installation,
official services and official progression remain separate. Game ownership and
community account authentication are independent requirements.

## What historical research establishes

The January 2024 [THE FINALS analysis by Reversing Thread](https://reversingthread.info/index.php/2024/01/10/the-finals-defeating-theia-packer/)
describes a particular Theia integration, including protected code pages and a
runtime component. Its author used a substitute driver during analysis and
recovered decrypted code. The article does not demonstrate a standalone,
driver-free replacement executable that launches reproducibly on another PC.
Its reported static keys, offsets and communication behavior are evidence for
that examined version, not for HELLDIVERS 2 build 26088801.

Recovering readable code and producing a working executable are different
results. Windows images also carry imports, relocations, thread-local startup
callbacks and exception metadata. A replacement must preserve whatever the game
requires from those mechanisms. The [Microsoft PE specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
describes these structures; a memory capture alone does not establish that they
remain valid in a new disk image or process.

## Evidence for the examined client

Read-only inspection identifies `preloader.dll` as an import of both the main
executable and `data/game/game.dll`. Both have TLS initialization callbacks. The
contained launch also observes `runtime.dll` as a mapped image and a separate
child process. These observations establish several bootstrap dependencies;
they do not identify the complete protection configuration or prove the same
decryption design as the older THE FINALS build.

The [startup investigation](client-startup.md) identified and corrected an
isolated-environment error: missing `ALLUSERSPROFILE` caused a literal variable
name to be used in a file path. With that variable redirected to private storage,
the directory operation succeeded. A subsequent bounded diagnostic still
captured the runtime-base execute fault, but its preserved status was now
`STATUS_ACCESS_DENIED` (`0xC0000022`) instead of `STATUS_OBJECT_PATH_NOT_FOUND`.

The [subsequent named-object comparison](client-startup.md#named-object-compatibility)
identified `NtCreateMutant` trying to create a named mutex under
`\BaseNamedObjects\Packer_...`. That operation was denied in the AppContainer;
the same operation and access mask succeeded in an ordinary unprivileged control.
A second contained fixture succeeded using its own private object namespace.
These results identify an incompatibility in the startup probe's environment.
They do not establish a driver requirement, integrity rejection or a working
game adaptation. The game has not advanced beyond this request, and engine
startup remains unobserved.

## Next engineering gates

1. Resolve the identified named-object namespace incompatibility in the startup
   environment, preserving network and user-data isolation. Verify advancement
   in the actual game before interpreting any later failure as a protection
   dependency. Do not change machine-wide namespace permissions to mask it.
2. Establish which initialization work the preloader/runtime performs for this
   exact build, including any code preparation and later runtime dependencies.
   Do not import old offsets or assume deleting an import produces valid code.
3. Demonstrate a reproducible user-mode startup through engine initialization.
   Identify any dependency on another process, device, session or machine before
   choosing an on-disk transformation or a launch-time adapter.
4. If a transformation is viable, require exact input hashes, separate output,
   verified output identity and recovery. Reject unsupported builds. Preserve the
   distribution boundary: launcher/integration components, not game executables.
5. Validate cold starts, a second PC, ship entry, mission load and mission return
   without a custom driver or researcher-installed state. Then verify the separate
   profile, service routing and multiplayer contracts.

The current decision is **plausible but unproven**, with startup research as the
next priority. A large delta could deliver a proven transformation; it cannot
substitute for discovering and satisfying the actual initialization requirements.
