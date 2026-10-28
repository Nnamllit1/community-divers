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

## Lua addons alongside the existing runtime

[Boot Mod Loader](https://github.com/pat-pat-patrick/boot-mod-loader/tree/96ff9818c51c669d8f690e01db49198a44453297)
provides an upstream Lua-only route whose checked-build list includes **26088801**.
Its boot replacement executes the original game boot, then starts addons after
the original Lua `init` returns. It exposes callbacks and engine Lua APIs;
its capabilities explicitly report `ffi=false` and `jit=false`. This is an
upstream compatibility claim, not a Community Divers game-startup test.

The inspected exact-build original boot chunk has empty Lua lifecycle callbacks.
Consequently, the order of its Lua `init` call does not establish the timing of
native account or backend initialization. The prepared copy's settings expose
PlayFab title IDs and boot/profile paths, but no demonstrated community backend
URL. A current native endpoint/config consumer or exposed API still needs to be
identified and tested with official-service and profile separation.

This route can inform features implemented through existing Lua APIs. It starts
after the preloader/runtime stage investigated here and provides no demonstrated
community-account bootstrap or backend-routing adapter. The reviewed HellForge
integration also relies on native routing and SDK/Cloud stand-ins for its older
supported build. Those requirements remain separate current-build engineering
work; successful Lua loading would not establish community-client compatibility.

## Native engine-plugin candidate

The reviewed [Native Bridge source](https://github.com/CowboyBingus/BingusNativeBridge/tree/b39184851b76ee6096095f4e46d163b3f1b0108b)
implements the engine's `get_plugin_api`/`setup_game` handshake and registers
three informational Lua functions. It provides no community backend adapter or
native module loader. Its mock host tests the declared interface, rather than
the current game's ABI, acceptance policy or initialization order.

The reviewed v0.1.1 DLL has no PE certificate and Windows reports it unsigned.
The project's pinned [signing policy](https://github.com/CowboyBingus/BingusNativeBridge/blob/b39184851b76ee6096095f4e46d163b3f1b0108b/CODE_SIGNING_POLICY.md)
also describes signing as planned and unsigned releases as unable to load.
A legitimately signed engine plugin remains a candidate alongside the original
runtime. Current-build acceptance, working callbacks and initialization early
enough for community service routing are all unverified. No bridge was installed
or executed in this assessment.

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

Read-only inspection of the current stock `level_generation` and `wwise`
plugins also finds `preloader.dll` imports and `get_plugin_api` exports. These
share the main executable/engine DLL's TLS-array/import-slot alias and empty
exception directory despite retained `.pdata` sections. Independent `dumpbin`
output agrees with the structural reader in 18 comparisons. These observations
extend the image inventory affected by the shared bootstrap architecture. They
do not establish which plugins are essential to startup or what preparation
their code requires.

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
Bounded static decoding and CPU-only observation have followed its initial path
to Windows thread/process-state access, as described below. Its complete control
flow and initialization contract remain unresolved. The presence of ordinary
user-mode mapping code in the preloader does not prove that later runtime code
has no driver, service or machine-specific dependency.

The immediate remaining investigation is the runtime's initialization contract:
what it prepares for each protected image, how it makes imports and exception
handling usable, and what dependencies remain after startup. No protection
checks were disabled, no game files were patched, and no new game launch was
performed for this structural investigation.

## Shared initialization record

The preloader copies **18 Windows API pointers** into the record passed to the
runtime. Their sources are explicit import-address entries, and the copies occur
before the first runtime dispatch. They cover process/thread creation, waiting
and process/system queries; file and directory operations; environment and
process-parameter handling; and registry/dialog functions. Consequently, the
runtime's small ordinary import table is not its complete interface to Windows.

The same initializer stores eight local image-relative pointers. Several point
to routines with window-message or hook-forwarding behavior, while others remain
unclassified. An address in an executable section can also point to data; it is
not automatically a callable function. Further static tracing identifies the
preloader base, actual runtime mapping base, runtime entrypoint, a support-module
base and an image-section handle. On native Windows, that module identity comes
from resolving the containing module of a captured exception instruction pointer;
it is not simply a fixed address. The section handle comes from actual image
section creation. The Wine path obtains these inputs differently.

The preloader attempts runtime placement relative to the selected support module.
Consequently, using the PE preferred base in a simulator is an explicit modeling
assumption, not a measurement of native placement. The complete record and the
lifetime of its objects remain unresolved.

This inventory is not evidence that every supplied function is used. It does
not establish the full record size, all initialization paths or
the runtime's lifetime requirements. Those distinctions matter when evaluating
whether a replacement can preserve startup rather than merely return success.

The inspected successful preloader paths retain this record across later
callbacks without rebuilding it or directly closing its section handle. Copying
the `ZwClose` address into the record is not itself a close operation. The runtime
receives a mutable record and can change the handle's lifetime; the retained field
does not prove that the handle is still valid or establish who owns its cleanup.

## Initial runtime state dependency

A bounded CPU emulator followed unchanged instructions from the runtime's declared
entrypoint, using the first-dispatch arguments identified in the preloader. It
reaches the current thread's environment block, its process-environment pointer,
and a process field identifying the executable image. Microsoft documents the
[thread environment block](https://learn.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-teb)
and its process-environment pointer. A separate C++ diagnostic verified the SDK
offsets and compared the image-base field with its own executable module handle;
that comparison inspected the diagnostic itself, not the game.

With only those pointer relationships modeled, two different initial register
seeds reach the same `CPUID` query. The surrounding static code closely matches
Microsoft's named `__isa_available_init` routine from the installed C runtime,
including vendor comparisons, processor-model masks and feature-selection data.
The binaries are not identical. This supports ordinary CPU-feature initialization
as the interpretation of this early query, not a protection rejection. Microsoft
documents [CPUID's processor-information behavior](https://learn.microsoft.com/en-us/cpp/intrinsics/cpuid-cpuidex?view=msvc-170).

Separate C++ diagnostics subsequently captured this machine's CPU-query results,
platform-query results and the 18 imported API addresses in their own processes.
The bounded model replays those measurements unchanged, along with initializer
literals established by static tracing. It does not execute game code natively
or forward simulated calls to Windows. Addresses observed in a diagnostic process
are not proof of their placement in a game process.

The model still uses a synthetic caller/stack and preferred image base. Imports,
main-image contents, most shared-record fields and Windows services remain
unimplemented. Unknown reads and external calls stop observation. Controls verify
these stops, read-only code and bounded execution. These are simulation limits,
not observed game failures. Agreement between two initial register cases does not
prove equivalence with every native startup condition. Modeled pointers remain
explicit assumptions; no successful API calls or protection-check outcomes are
invented to advance the trace.

## Runtime table consumption

With those measured inputs, the runtime requests a 144-byte copy of the supplied
API table into its own data. The selected memory-copy path uses AVX instructions
that the pinned emulator could not execute correctly. Independent synthetic
controls reproduced both unsupported vector moves and incorrect upper-vector
clearing; some instruction-length hints were also truncated. These were observer
defects, not evidence of a protection failure in the game.

A narrow instruction-semantics helper now handles the observed vector moves and
upper-vector clearing. It preserves memory permissions and unknown-input stops,
and decodes only within contiguous executable memory. Verification includes 23
helper controls, eight instruction-fetch controls, 27 integrated observer checks
and a separate native test over diagnostic-owned buffers.

Two different initial register cases now copy all **18 pointers** correctly and
stop at the same subsequent eight-byte read of the support-module identity.
Each trace observes 57,308 instructions; the copied table matches the measured
input byte for byte. This establishes table consumption within the model, not an
invocation of any supplied Windows function or complete runtime initialization.
In that comparison, the support-module value is left unresolved rather than
replaced with an assumed address.

## Support-module parsing

Read-only snapshots at that stop allowed an independent static evaluation of the
next data flow, keeping the unknown module value symbolic. Accounting for all
intervening writes and overlapping destinations established that the value is
saved twice and subsequently passed to another runtime helper. This resolves
the argument path without assigning an invented module address.

A separate C++ diagnostic then reproduced the preloader's exception-based module
lookup inside its own process. The captured fault instruction resolved to that
process's `ntdll` module. A second diagnostic captured only its 4,096-byte loaded
PE headers, verifying readable committed image memory and matching module/file
identities. Neither diagnostic loaded or executed game or protection code.

Replaying that measured identity and header data advances both initial register
cases through DOS and PE header reads, then to the export-directory
`NumberOfNames` field. Each bounded trace observes 82,399 instructions and retains
the matching 18-pointer table. The export-directory contents remain unmodeled,
so observation stops before reading that field. This establishes the start of
export metadata parsing, not a resolved function, an API call or complete module
validation.

The header mapping is read-only and non-executable; reads beyond captured bytes
stop. Nine additional controls cover those boundaries and reject changed header
bytes, mismatched capture references, missing page coverage and inconsistent PE
identity fields. Separate-process addresses, synthetic stack state and preferred
runtime placement remain explicit assumptions. The observations do not establish
the corresponding native game-process state.

## Export-name observation

A further own-process C++ diagnostic captured the declared **80,551-byte**
`ntdll` export-metadata range, with module/file identity and readable,
non-executable page checks. Replaying that range read-only lets the runtime
enumerate all **2,522** named exports in the model. Recorded comparison operands
identify the first two selected bytes as `Z` and `w`: **490** names match that
prefix. An independent metadata parser checks these counts and the recorded
operand examples. Export metadata reads do not establish calls to those functions.

Two observer limitations were resolved before continuing. A thread-state read
comes from a routine whose decoded instructions match the installed MSVC stack
probe. The model supplies the lower boundary of its already mapped synthetic
stack; native guard-page growth remains unimplemented. A later two-byte
`MOVSXD` encoding executes correctly in the pinned CPU emulator but is rejected
by the pinned disassembler. An authored native control and 48 emulator
comparisons verify its register and flag behavior. The observer supplies only
the missing decode metadata and executes the original instruction unchanged.
Other undecodable instructions continue to stop observation.

The subsequent scan constructs an address/value table. In two recorded stack
snapshots, its 154 visible rows match the prefix of an independently
address-sorted list of 490 distinct `Zw` exports. For 49 visible names of length
9 through 16, the value matches a statically decoded full-name hash. The other
rows and hash-length paths are outside that bounded proof. This does not yet
establish how the table selects a native operation.

## First system-call boundary

Longer observations exposed a timing issue in the observer: asynchronous timeout
resumption could count an instruction hook before its instruction executed.
Equal hook budgets then ended five instructions apart. The observer now uses
fixed 100,000-instruction slices and treats an asynchronous slice timeout as a
terminal observation. Controls compare different slice sizes and verify that
timeout stops are never resumed. All 23 export-observer controls pass.

With that correction, both initial register cases reach the same `SYSCALL`
instruction after **52,593,507** observed instructions. Their recorded input and
metadata reads, instruction tails and slice signatures agree; the independent
audit also passes seven changed-evidence controls. The API table still matches
all 18 supplied pointers. Observation stops before executing the system call.

The saved service number is **4**. A separate own-process C++ diagnostic reads
32 bytes from the measured `NtWaitForSingleObject`/`ZwWaitForSingleObject` export
without invoking it. Its entry stub loads the same number before `SYSCALL`.
For this Windows module identity, this identifies a wait operation with handle
`-1`, alertability disabled and a captured timeout value of zero. Microsoft
documents [the current-process pseudo-handle](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentprocess)
and [zero-timeout wait behavior](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntwaitforsingleobject).
These describe a nonblocking current-process wait request in the model. Its
purpose in the runtime remains unresolved; the request is not itself evidence
of an integrity rejection or a custom-driver requirement.

The native diagnostic passed ten own-process data-read and refusal controls.
It measures this machine's Windows module, not the game's native process state.
The runtime's earlier derivation of service number 4 remains incompletely traced.
That observation supplied no wait result or post-system-call state. The later
measured continuation below establishes a narrow contract for this one request.

## Measured wait continuation

An authored C++/assembly diagnostic executed the fixed, nonalertable,
zero-timeout current-process wait in its own process. Its 256 cases cover all
64 arithmetic-flag combinations, attempted IF clear/set and two register seeds.
It verifies the Windows module and native entry bytes before measurement.
Every case returns `STATUS_TIMEOUT` (`0x102`); ordinary native and Win32 controls
also return timeout for the current process and success for an owned signaled
event. These are actual diagnostic results, not substituted successful checks.

The diagnostic also measures the registers and flags immediately around
`SYSCALL`. RCX becomes the following instruction address, R11 receives the prior
flags, and those flags remain unchanged on return. Several other volatile
registers change. The model replays the measured mapping for this exact request,
with the model's continuation address instead of an address from the diagnostic.
The result is pinned to the measured Windows module; other operations still stop.

The comparison identified another observer limitation: its initial IF flag was
clear, and its ordinary `POPFQ` behavior allowed that privileged bit to change.
The native user-mode controls establish that CPL3 with IOPL0 preserves IF. A
narrow layer now starts with user-mode flags and handles only the verified safe
`POPFQ` domain. Unsupported flag bits, prefixed forms and unreadable operands
stop observation. All 128 safe flag cases and 13 flag/provenance controls pass.
The wait layer additionally passes 64 measured flag cases and 13 refusal checks,
including changed arguments, unwritten or partial timeouts and exhausted budgets.

With this layer, both seed cases advance through the first request and stop
before a second `SYSCALL`, after **52,595,848** observed instructions: 2,341
instructions beyond the first request. The zero timeout has explicit provenance
from an eight-byte immediate-zero store in the runtime. The observed instruction
tails, slice counts and call operands agree. Three preserved caller registers
retain their declared seed values; full raw-state convergence is not established.

The second request specifies service number 4, but uses handle `-10000` and a
null timeout pointer. A separate diagnostic measures this exact tuple in an
owned child under a retained-handle deadline. All 256 cases return
`STATUS_INVALID_HANDLE`, with RAX exactly `0x00000000C0000008`. Ordinary native
and Win32 controls agree on the invalid-handle outcome. Its separate validator
rejects twelve changed-evidence cases; the first native evidence remains intact.

A second narrow model accepts that tuple only after the first measured wait.
Its 64 flag cases and nine refusal checks pass, including request order, repeat
requests, wrong service numbers and stack alignment. Both seed observations now
advance to a third `SYSCALL` after **52,597,686** instruction hooks. The paired
audit verifies three phases, both measured output mappings and terminal refusal
of the new request; fifteen evidence-mutation controls are rejected. Declared
seed-dependent state remains explicitly reported rather than called convergence.

The third request uses service number 4, current-process handle `-1`,
alertability disabled and timeout pointer `-10000`. Another bounded, owned
diagnostic measured 256 normal returns with full RAX
`0xFFFFFFFFC0000005` (`STATUS_ACCESS_VIOLATION`), with no delivered exception.
Its sign extension differs from the preceding result and is preserved exactly.
The third validator rejects sixteen changed-evidence cases, including incorrect
status width and treating an exception as a normal return.

The three-contract model passes 64 third-tuple flag cases, ten refusal checks
and passive-observation controls, alongside the earlier controls. Both seed
observations advance through the three measured results and stop at a fourth
system call after **52,600,167** observed instruction hooks. A separate audit
verifies all three actual post-return general-register/flag readbacks and six
bounded guest-stack snapshots, with 29 evidence-mutation controls rejected.
These snapshots are diagnostic data, not executable process checkpoints.

The new status-access observations also establish that the original stack slots
are reused. A later value in one slot cannot be treated as the earlier result.
Some inspected arithmetic involving a status cancels independently of that
status. Later bounded evaluation identifies equality comparisons against the
second and third low-32-bit error codes. Their selected continuations and
higher-level purpose remain unresolved; no removable integrity check is
established.

The fourth request specifies service `0x1E6`, identified against the measured
`NtWaitForKeyedEvent` entry, with handle `-10000`, NULL key, alertability disabled
and NULL timeout. An owned diagnostic measures 256 normal returns with full RAX
`0x00000000C0000008`; eighteen changed-evidence controls reject. Its exact model
passes 64 flag cases and ten refusals. Both seed observations now advance through
four measured returns and stop before a fifth request after **52,602,849**
instruction hooks. The independent paired audit verifies all four post-return
general-register/flag maps and eight bounded guest-stack snapshots, with 42
changed-evidence controls rejected. Seed-dependent registers and stale stack
spills remain explicitly identified; full state equality is not established.

The fifth request uses the same service, current-process handle `-1`, NULL key,
alertability disabled and the previously defined zero-timeout address. A separate
owned diagnostic measures 256 normal returns with full RAX `0x00000000C0000024`
(`STATUS_OBJECT_TYPE_MISMATCH`); twenty-two changed-evidence controls reject.
This establishes another narrow native contract. Its actual model continuation
has not yet been verified. The runtime's subsequent preparation of protected
images, engine startup and startup without the original runtime remain
unverified. Further AppContainer adaptation stays deferred.

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
