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
They do not establish a driver requirement or integrity rejection.

The later [private-namespace comparison](client-startup.md#private-namespace-startup-comparison)
advanced the actual runtime beyond this mutex request, with a verified private
handle and unchanged native return semantics. This is diagnostic argument
adaptation, not a deployable patch. Subsequent runs exited with `0xC0000244`
before the engine loaded; the exit's caller and cause remain unattributed.
Engine startup remains unobserved.

## Current-build startup structure

An exact-hash, read-only inventory of the four bootstrap images was compared
against Microsoft `dumpbin` header, import, TLS and export output. It did not
execute an image, recover decrypted code or modify game files.

| Image | Observed on-disk structure | Integration consequence |
| --- | --- | --- |
| Main executable | Its TLS callback array occupies the same import-address slot as the preloader import. The exception-directory entry is empty although a `.pdata` section exists. | Initialization depends on the preloader before ordinary application startup. The runtime treatment of exception metadata remains to be identified. |
| Engine DLL | The same TLS/import-slot arrangement; an empty exception-directory entry alongside a `.pdata` section. | The dependency also applies when the engine DLL is loaded; solving only the executable's launch is insufficient. |
| Preloader | The imported export and DLL entrypoint lie in an on-disk section marked read-only, without execute permission. Other native startup code is present separately. | The on-disk entry addresses alone do not describe the eventual execution path. Earlier debug evidence recorded a handled first-chance execute fault at this entry before runtime initialization continued. |
| Runtime | Multiple executable sections, its own exception metadata and a small ordinary import table. | Static imports do not provide a complete inventory of its runtime dependencies. Mapping this image is not proof of engine initialization. |

The apparent callback values in the main/engine files are import-ordinal entries,
not ordinary on-disk function pointers: the callback array aliases the import
address table. Windows resolves that table when loading imports. This is a
structural link between early initialization and the preloader, not evidence that
the callback list is corrupt. The
[PE specification](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
defines the import-address table, TLS callback array and exception directory.

These observations rule out treating the preloader as an unrelated launcher
component. They do not establish which code or data is transformed, whether
exception tables are registered dynamically, which checks are essential to
initialization, or whether a driver-free replacement is feasible. The next
architecture task is to identify that initialization contract and any continuing
runtime dependency. A deployable patch remains unimplemented.

## Preloader-to-runtime handoff

The x64 exception metadata explicitly covers the preloader's non-executable
entry stubs. Both entries select a handler that reads its associated destination,
updates the saved instruction pointer and returns the continue-execution
disposition. Their destinations are executable startup routines. This explains
why an execute access violation at the stub can be part of initialization, rather
than a terminal crash or evidence of an anti-cheat rejection. It is consistent
with the earlier debug log, which continued beyond that first-chance exception.

The metadata was checked independently through Windows DbgHelp using a read-only
data mapping, with no execution or DLL initialization. The Windows SDK also
confirmed the exception-context field layouts used in the interpretation.
Microsoft documents the relevant
[x64 exception-handler and unwind structures](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170).
The disassembler's hybrid-image unwind display selected ARM64 data; that display
was not used as independent confirmation of the x64 records.

The native startup routines share a once-initialized runtime handoff. Static
control flow identifies file/image preparation, address relocation and memory
protection operations, followed by a call to the runtime's declared entrypoint
with a shared initialization record. Later callbacks use the same cached runtime
dispatch address. These are responsibilities that a compatible replacement would
need to account for; a successful return from a stub would not demonstrate them.

The runtime entrypoint leads into code with repeated indirect control transfers.
Only a bounded, unchanged instruction slice was examined; its complete control
flow and initialization contract remain unresolved. The presence of ordinary
user-mode mapping code in the preloader does not prove that later runtime code
has no driver, service or machine-specific dependency.

The immediate remaining investigation is the runtime's initialization contract:
what it prepares for each protected image, how it makes imports and exception
handling usable, and what dependencies remain after startup. No protection
checks were disabled, no game files were patched, and no new game launch was
performed for this structural investigation.

## Next engineering gates

1. Establish which initialization work the preloader/runtime performs for this
   exact build, including any code preparation and later runtime dependencies.
   Do not import old offsets or assume deleting an import produces valid code.
2. Distinguish these requirements from diagnostic-environment restrictions.
   AppContainer is an optional research mechanism, not a loader prerequisite.
   Further work on its compatibility is deferred. The observed mutex succeeds in
   a diagnostic comparison, but the later early exit remains unattributed; it
   cannot be used as proof of a protection dependency. Future dynamic comparisons
   still need separate user data and verified exclusion of official services.
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
