# Current-client startup investigation

The current client adapter is unfinished. Steam build **26251122** has a fresh
static bootstrap inventory. The live startup observations on this page were
collected on build **26088801** on 27-29 October 2026; they have not been validated
on the newer binaries. This does not describe a playable community client. See
the [game-version target policy](client-integration.md#game-version-target-policy).

## Latest-build static inventory

A separately prepared copy of build **26251122** passes full content verification
for 168 depot files totaling 24,128,702,003 bytes. Its eight inspected module
identities match the build catalog. This establishes file-copy integrity; startup,
profile/Cloud isolation and official-network isolation remain unverified.

A read-only comparison of all eight inspected modules, cross-checked against
Microsoft `dumpbin` output, finds unchanged normal import descriptors, TLS directory
fields and callback arrays. The executable, engine and both stock engine plugins
still import preloader ordinal 1, and their on-disk TLS callback storage still
overlaps that import's address-table slot. This preserves a concrete dependency;
it does not establish unchanged initialization behavior.

Several declared entry points and the runtime's exception table changed:

| Metadata | Build 26088801 | Build 26251122 |
| --- | --- | --- |
| Executable PE entry RVA | `0x12EA6C8` | `0x12EA554` |
| Runtime PE entry RVA | `0x560040` | `0x519D00` |
| Wwise plugin PE entry RVA | `0x3C1308` | `0x3C1318` |
| Runtime declared exception-table entries | 28,601 | 28,881 |

The preloader's declared entry remains in a non-executable section, but its
`.text` and `.rdata` section bytes changed. The executable, engine and both stock plugins
still have no declared exception directory despite containing `.pdata` sections.
All 16 old/new module identities remained unchanged across the inventory pass.
These are file structures, not observations of loaded code or exception dispatch.
Earlier instruction addresses, native-call observers and runtime results require
fresh validation before use on build 26251122.

A literal comparison of the top-level `data/settings.ini` changes only its build
and audio-version lines. This does not locate native settings consumers or
establish a community-backend configuration route.

## Bootstrap dependencies

For build **26088801**, read-only PE inspection and a contained startup probe of
the independently prepared copy found:

- `bin/helldivers2.exe` imports `preloader.dll` and has TLS initialization callbacks.
- `data/game/game.dll` also imports the preloader and has TLS callbacks.
- The preloader starts `runtime.dll` as a **separate process** during initialization.
  The `.dll` filename must not be taken to mean it loads only into the game process.

The last point was observed in the contained process tree, where the child process
reported `runtime.dll` as its main module. The executable, preloader, runtime and
engine are separate integration dependencies. Lua support or unchanged networking
SDK binaries do not establish a working native bootstrap.

## Offline probe conditions

AppContainer is an experimental isolation mechanism for these diagnostics, not
a prerequisite imposed by HellForge, a product requirement or a way to make the
client trusted by anti-cheat. Its restrictions can introduce failures absent
from an ordinary desktop process. Further AppContainer adaptation is deferred
while the current build's protection/bootstrap architecture is investigated.
Data and official-service separation remain requirements for eventual deployment.

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

A subsequent bounded probe captured the runtime child's main thread in the
Windows dialog message loop, with frames in `DialogBoxParamW`. Static resources
include a crash-report dialog and a hardware-test dialog. At that stage the active
dialog and underlying exception had not been identified. This distinguished the
observed wait from engine startup without assigning an unverified failure cause.
A separate fixture proved descendant cleanup after the parent had already exited.
The probe verified an empty job before restoring access and removing its profile;
all 180 checked access descriptors and five bootstrap file hashes were unchanged.

A bounded diagnostic launch using standard Windows debug events recorded seven
first-chance exceptions in the root process, including an execute access violation
at the mapped runtime image base. No second-chance exception was reported before
the probe's timeout. These notifications are not themselves proof of an unhandled
crash: application exceptions were passed back for normal handling. Only the first
startup breakpoint in each process's `ntdll` was consumed by the debugger. The
runtime child again waited in `DialogBoxParamW`, and no engine module was observed.

Synthetic fixtures verified handled exceptions, an application breakpoint, an
unhandled child exception and descendant cleanup before this diagnostic launch.
The debugged fixture and its child also retained the zero-network/file-access
restrictions. The game probe left an empty job; access descriptors, copy paths and
the five bootstrap hashes were unchanged after cleanup. Debugger instrumentation
changes execution conditions, so this result does not prove that the same exception
sequence occurs during an ordinary launch or identify the active dialog.
See Microsoft's [exception continuation semantics](https://learn.microsoft.com/en-us/windows/win32/api/debugapi/nf-debugapi-continuedebugevent).

A run without debug-event instrumentation captured the runtime child's process
parameters: its command line contained only its image path, with no additional
arguments. It reached the same dialog wait. A subsequent diagnostic captured the
root's execute-fault context, and a matching user-visible crash report confirmed
that the runtime was reporting that fault. The preserved registers included
`STATUS_OBJECT_PATH_NOT_FOUND` (`0xC000003A`); the failed lookup is not identified.
The report separately failed to create its dump directory with
`STATUS_ACCESS_DENIED` (`0xC0000022`). These are distinct observations, not proof
that the dump path caused the original execute fault.

The report exposed an error in the probe's temporary-directory setup. Windows
rewrites an AppContainer's `LOCALAPPDATA`, `TEMP` and `TMP` beneath its per-container
folder. Overriding the launch environment had not created that redirected tree.
Earlier fixtures wrote explicit private paths and therefore missed this case.
The corrected setup creates the private per-container folders after applying their
inherited access and integrity settings. New fixture checks verify actual
`APPDATA`, `LOCALAPPDATA`, `TEMP`, `TMP` and `GetTempPathW` file writes and reads in
both parent and child, alongside the existing outside-file and network denials.
See Microsoft's [AppContainer profile and environment behavior](https://learn.microsoft.com/en-us/windows/win32/secauthz/implementing-an-appcontainer#creating-the-profile).

A normal diagnostic run after that correction still reached the runtime dialog
without observing the engine. The probe cleaned up both processes and left all
checked copy permissions, paths and bootstrap hashes unchanged. Actual game dump
creation has not been reverified, and the underlying startup failure remains open.
A later debug run captured the same runtime-base fault and path-not-found status
with the corrected folders. Its final process/ACL receipt was interrupted, so it
is not counted as a complete probe. Subsequent inspection found no surviving probe
processes; the exact temporary profile and its access rules were removed and all
five bootstrap hashes remained unchanged. The pre-run ACL snapshot was not retained.

## Launch arguments and recoverable diagnostics

A comparison supplied an explicit `--bundle-dir` pointing at the prepared copy's
`data` directory, together with `--release`. These options came from the older
loader's launch contract; they were tested as a hypothesis, not assumed current
requirements. The captured root command line confirmed both options were present.
The same runtime-base execute fault, path-not-found status and child dialog wait
remained. No engine module was observed. The bounded run ended with an empty job,
all 180 access descriptors restored and all five bootstrap hashes unchanged.
Supplying these arguments is therefore not a startup fix for this contained run.

The probe now persists its complete pre-run access/hash baseline and a separate
profile/process identity journal before changing access. A synthetic interruption
check observed a fixture parent exit with a descendant still present, then forcibly
stopped the owning probe. Recovery refused to act while the owner was alive; after
termination it removed only the recorded profile's access rules and profile, with
no remaining fixture processes or rules. The ordinary parent/child isolation and
actual temporary-directory write checks also passed with journaling enabled.
This improves recovery from a lost tool session; it does not establish game startup.

An empty ETW session capability check returned `ERROR_ACCESS_DENIED` for the
ordinary token. The subsequent trace used a separately authorized elevated
observer; the game retained its unprivileged, zero-capability token. Windows
documents the controller permissions in
[StartTrace](https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-starttracew)
and [EnableTraceEx2](https://learn.microsoft.com/en-us/windows/win32/api/evntrace/nf-evntrace-enabletraceex2).
No tracing permissions or group memberships were changed.

## File-lookup trace

The realtime observer retained file-create and completion metadata for the exact
root process instance. The kernel provider did **not** honor the requested PID
scope filter in these checks. Its callback rejected unrelated event headers
before decoding payloads; it wrote no ETL and retained no unrelated filenames or
file contents. A fixture correlated a deliberate missing-file lookup with its
completion status and verified that descendant and observer control paths were
absent from the retained events. Event-loss and decode-error counters were zero.

The game trace identified two `STATUS_OBJECT_PATH_NOT_FOUND` results for a literal
`%ALLUSERSPROFILE%\Packer` path beneath the executable directory. The isolated
environment lacked `ALLUSERSPROFILE`. A corrected fixture verified read/write
access through both `ALLUSERSPROFILE` and `ProgramData`, redirected to the same
private directory, while preserving the outside-file and network denials in
parent and child processes.

With that environment correction, the game resolved `Packer` beneath private
ProgramData: its first lookup returned `STATUS_OBJECT_NAME_NOT_FOUND`, followed
by a successful operation and creation of that directory. The root and runtime
child still remained until the observation timeout, the child retained the same
dialog-wait stack, and no engine module appeared. This removes an observed
probe-environment error but does not fix startup or identify the remaining cause.
Both game traces stopped their ETW sessions, emptied their owned jobs, removed
temporary profiles and restored all 180 checked access descriptors; all five
bootstrap hashes and the copy's path inventory remained unchanged.

A subsequent debug-event comparison with the corrected environment captured the
same runtime-base execute fault and caller, with `STATUS_ACCESS_DENIED`
(`0xC0000022`) now preserved in the status registers. It recorded seven
first-chance exceptions, no second-chance exception and no engine module before
the imposed timeout. Cleanup again left an empty job, restored the 180 checked
access descriptors and preserved all five bootstrap hashes. At that stage the
specific denied operation was unidentified; the comparison below subsequently
attributed it without establishing a driver dependency or anti-tamper rejection.

## Named-object compatibility

A bounded fault-context capture identified an explicit reporting path in the
runtime. Its recorded caller instructions matched the exact on-disk runtime
bytes. Skipping that reporting path would not establish successful initialization.
A nearby native-call return supplied a narrower diagnostic candidate.

A subsequent probe used processor breakpoints on that candidate call in the
owned root thread, recording its arguments and return status. It observed
`NtCreateMutant` requesting a named mutex under `\BaseNamedObjects\Packer_...`
with `MUTEX_ALL_ACCESS`, and receiving `STATUS_ACCESS_DENIED`. The operation was
identified against the examined Windows build's native API stub rather than
assuming system-call numbers are stable across versions.

The observer was checked against synthetic named-object requests before the game
run. It paired entry and return in the same process/thread and confirmed the
fixture received the same denied status. It did not replace the result, suppress
application exceptions or modify game bytes. Processor breakpoints do change
debug registers and execution conditions, so this is instrumented evidence.

Controlled comparisons then found:

- The same mutex operation and access mask succeeded under the ordinary,
  unprivileged account; its unique control object was immediately closed.
- The operation was denied in the zero-capability AppContainer when addressed
  through `\BaseNamedObjects`.
- A mutex in the AppContainer's own named-object namespace succeeded under the
  same contained token without granting additional capabilities or permissions.

These comparisons establish a startup-environment incompatibility for this
request. Microsoft documents that AppContainer applications have their own
[named-object path](https://learn.microsoft.com/en-us/windows/win32/api/securityappcontainer/nf-securityappcontainer-getappcontainernamedobjectpath)
and that ordinary session/global objects are unavailable to them by default.
The private-namespace game comparison below subsequently applied this idea to
the recognized mutex request. Neither the fixtures nor the ordinary-account
control proves the game can start without its remaining runtime dependencies.

The instrumented game still reached its error-reporting path and child dialog
before the imposed timeout; no engine module appeared. Both owned processes were
removed, the temporary profile was deleted, all 180 checked access descriptors
were restored, and the copy inventory and five bootstrap hashes were unchanged.
Further comparisons must distinguish namespace compatibility from actual
protection dependencies while retaining network and user-data separation.

## Private-namespace startup comparison

A diagnostic compatibility adjustment redirected only the recognized runtime
mutex's name to the current AppContainer's private namespace. It kept the
requested access, initial ownership and caller-supplied security descriptor,
used temporary argument storage, and left the native result unchanged. The
created handle was queried to verify that it referred to the expected private
object. Temporary storage was released after the call. This changes an in-memory
call argument during a debug session; it is not an on-disk patch or a deployable
client adapter.

Synthetic checks verified actual handle creation, native duplicate-name errors,
unchanged security-descriptor bytes, untouched unrelated names and temporary
storage cleanup. The current harness also passed its parent/child file and
network isolation controls. No machine-wide namespace permission was granted.

The real runtime then created its mutex successfully, and its previous
runtime-base error-reporting fault and child crash dialog did not recur in these
comparisons. One run remained active until the imposed 15-second limit. Two
subsequent runs, each allowed up to 45 seconds, exited independently within a few
seconds with `0xC0000244`; neither reached the engine.

Windows names this status `STATUS_AUDIT_FAILED` in its
[NTSTATUS definitions](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-erref/596a1078-e883-4972-9bbc-49e60bebca55).
A process can select an exit value, so the label alone does not identify the
terminating component or establish a full system audit log, an integrity failure
or an anti-cheat decision. No Windows auditing settings were changed.

A separate processor-breakpoint observer successfully recorded the synthetic
fixture's ordinary `NtTerminateProcess` shutdown calls with their unchanged
arguments. It recorded no such call in the game comparison. Its coverage is
limited to the observed API entry; this does not prove external termination or
identify the game's exit path. This exit remains an unattributed observation
from the restricted environment, not the current investigation's prerequisite.

The retained logs preserve all debug-event headers for those two exits, but
no terminal register context, caller stack or dump. Their saved register/code
snapshots belong to an earlier preloader execute fault, before mutex success.
Individual thread-exit codes were not serialized. Windows' process-exit debug
record supplies the exit code rather than a terminating caller, so the missing
context cannot be recovered from that record alone. See the
[process-exit](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-exit_process_debug_info)
and [thread-exit](https://learn.microsoft.com/en-us/windows/win32/api/minwinbase/ns-minwinbase-exit_thread_debug_info)
record definitions.

An authored Windows control demonstrates this ambiguity: a worker calling
`ExitProcess` and a last worker calling `ExitThread` both produced `0xC0000244`
on a noninitial thread. Their event-kind/thread-role/final-status sequences
matched when individual thread-exit codes were omitted; those codes distinguished
the two controls. This does not identify the retail client's exit mechanism or
extend the native-entry observer's coverage.

A later capture version records each actual debug-event continuation result,
exception contexts before and after the debugger's handling, and individual
thread-exit codes. It observes the ordinary system-library entries for both
`NtTerminateProcess` and `NtTerminateThread`. Eight authored control cases pass,
including 34 breakpoint-ownership checks, exception forwarding, the existing
namespace comparison, isolation and descendant cleanup. All 366 recorded event
continuations succeeded. The authored last-worker `ExitThread` control also
reaches process-termination entries internally on the examined Windows build;
fixture names alone do not identify the final native API.

A fresh retail comparison with that capture version again created the private
mutex successfully and exited with `0xC0000244`. All 43 event continuations
succeeded. Six unowned first-chance exceptions were forwarded with unchanged
captured contexts; no second-chance exception or imposed timeout was recorded.
The initial thread and two other threads each reported `0xC0000244`, followed by
the process exit with the same status on another thread. This distinguishes the
run from the exact authored control whose initial thread exited with zero, but
does not establish a terminating caller or cause.

Both ordinary termination-entry observers were armed on four threads and
recorded no entry hits. Their coverage does not include direct system calls,
other code copies or external termination, and the saved events do not prove the
debug registers remained armed until exit. No terminal caller context was
captured. The run preserved all eight catalogued module identities and the copy
inventory, restored all 180 saved access descriptors, removed the temporary
profile and left an empty job. Engine startup remains unverified.

These game comparisons ended with empty jobs, removed temporary profiles,
restored access descriptors, unchanged copy inventories and unchanged bootstrap
hashes. Engine startup and general namespace compatibility remain unverified.

A subsequent combined debug/file trace recorded an access denial for a runtime
cache beneath private ProgramData, followed by many Windows catalog-file opens.
The observer reached its 10,000-record limit and the harness stopped the target.
This is an incomplete trace, not another independently observed game exit. The
ETW session was removed; recovery verified the profile was removed, no residual
access rules remained, and all 180 access descriptors, copy paths and five
bootstrap hashes were unchanged. The cache denial's cause and relevance to
startup remain unproved; it does not establish an integrity rejection.

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

## Native catalog file observation

A bounded native trace now records actual entry and return at runtime RVAs
`5B3C26` and `5B3C28`, with the system service identified against the measured
Windows `NtOpenFile` export. It pairs the first 32 observed calls: 31 return
success, and one returns `STATUS_OBJECT_NAME_NOT_FOUND` (`0xC0000034`). That miss
is for `catcache4.bin` under the diagnostic's private `ProgramData\Packer`
directory. The runtime continues with successful file opens after the miss.

Queries on duplicated existing handles identify the Windows
`System32\CatRoot\{F750E6C3-38EE-11D1-85E5-00C04FC295EE}` directory. Twenty-nine
captured file-by-ID requests succeed. Two returned handles resolve to `1.CAT`
and a Windows Composition catalog in that directory, with file IDs matching
the requested binary references and volume metadata. The counted name fields
are binary IDs under the documented
[file-open options](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwcreatefile).
No unknown file was opened by ID in the host to obtain these names.

Fourteen authored cases pass, including 38 reader checks, 34 checks of termination
observer ownership and 40 file-observer checks. Controlled file calls preserve
their baseline statuses and payloads. A failed call's stale output handle is not
queried as a successful result. Worker controls verify handle inheritance,
metadata identity, timeout termination and cleanup. All 493 fixture continuations
succeeded.

The retail run records 107 successful event continuations, exits with
`0xC0000244` and completes verified cleanup. The file observer retires at its
32-pair limit; subsequent calls are unobserved. The earlier periodic snapshot's
exact file ID does not occur in this captured prefix; its file remains unresolved.
The missing cache and catalog opens do not establish the later exit's cause,
signature-verification outcome, a driver requirement or engine preparation.

This diagnostic transfers the original namespace observer's slots only after
its first verified mapped mutex pair and cleanup. Later calls at that old site
are no longer observed or adapted. The termination observers retain their own
slots. Metadata workers query existing duplicated handles without rewriting
requested file arguments or returned values, but those references can briefly
extend object and sharing lifetimes. These are instrumented observations;
equivalence of all later behavior and timing remains unverified.

## OS exit-tracing controls

A native diagnostic now separates a normal coordinator and targets from a
one-time elevated observer. Six self-created fixtures cover `ExitProcess`,
main-thread `ExitThread`, parent-requested `TerminateProcess`, an intentional
timeout, a numeric `0xC0000244` exit and a two-thread process exit. The high-bit
case supplies that numeric value to `ExitProcess`.
The observer uses a fresh real-time Windows session requesting
[process/thread exit stacks](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/stackwalk).
Recorded tokens confirm that the coordinator and all targets are non-elevated
at medium integrity, with the observer elevated at high integrity. All six
process and primary-thread exit codes, plus the additional worker's exit code,
match their expected values. The jobs emptied, session stopped and consumer joined.

All 129 pure-data controls and 16 independent negative audit cases pass. The
capture contains 26 owned event records and 13 matched stacks with 173 nonzero
decoded addresses. Original process/thread lifetimes, authentication, 910 input
identities, durable journals and raw stack bytes were independently checked.

Live module snapshots were taken while initialized fixtures waited at exit
gates. Module-file pins were released before gate release or requested
termination. Measured module bases, executable sections and queried memory
regions account for 54
user-range addresses as module/RVA candidates, including 21 in the self image.
Three frames also match declared function ranges starting at the authored
exit-helper exports. These metadata matches do not establish live code bytes,
debug-symbol locations or a terminating requester. The other 119 addresses are
in the upper half of the address space and remain unmapped; every process-exit stack contains
only upper-half addresses in this run.

All end-event emitters are victims, including the parent-requested termination.
The separate coordinator module map is not used for those victim stacks. Stack
matching uses the original event's
[timestamp and process/thread fields](https://learn.microsoft.com/en-us/windows/win32/etw/stackwalk-event),
with a final ambiguity check and a requirement that the selected emitter's
module snapshot precede the matched end event. Snapshots remain time-qualified;
they do not prove that mappings persisted or bytes stayed unchanged until exit.

The session reports zero lost events and buffers at stop. Separately, the
observer discards 363 unrelated-target events and 178 unmatched or pre-event
stacks, and records 74 cancelled callbacks during shutdown. The cancelled
callbacks have not been classified as unrelated or lost events. Complete
attribution is not established. Observer/coordinator failure-injection and
malformed-IPC paths remain untested live.

This collector still has only fixture evidence. Retail process-tree admission,
observation before loader initialization and the earlier game's `0xC0000244`
exit cause remain unverified.

## Early image-load observation

A separate native diagnostic uses ordinary Windows debug events to retain the
original process and thread capabilities before continuing its own targets.
It records actual executable/DLL bases from
[creation and image-load events](https://learn.microsoft.com/en-us/windows/win32/debug/debugging-events),
with the supplied file object's identity, hash and bounded PE metadata. It does
not require an initialized module list or an authored readiness gate. This probe
contains no native-call interceptors, instruction offsets, code writes or ETW
observer. The controller runs without elevation; its targets use a fresh
zero-capability AppContainer and an atomically assigned, owned job.

For the fixed-build game diagnostic, the execution policy binds the main
executable and one selected stock runtime image to their held, hash-verified
file identities. The original root
must match the main executable. Descendants must match one of the selected
identities and pass the original-handle, creation-time, job, token and metadata
checks before continuation. The implementation requires an eligibility matrix
for all eight actual catalog images before creating the game target; only the
main executable and runtime qualify as child executables.

All 104 metadata and policy controls and nine authored runtime cases pass. They
cover an exit during TLS initialization before the supported `wmain` route, two generations of
an owned DLL's load/unload, forwarded TLS/application breakpoints, handled and
unhandled exceptions, a two-process timeout and a diagnostic failure immediately
after real process creation. Two of these cases compare admission and rejection
of an authored child with the same executable bytes but a distinct file identity.
An additional CPU case stays alive beyond the earlier 15-second cutoff, then
exits naturally with status 60 under a 20-second case budget.
Independent checks join 312 continued events, 12 original process lifetimes,
42 terminal thread capabilities and 216 image generations. The nine profiles
were removed and the 18 saved directory DACLs restored. The low-integrity label
on newly created diagnostic data is retained.

The admitted child writes a verified 16-byte marker from its TLS callback. In the
rejected case, its file identity, hash, path and actual load base are retained as
observed metadata only. A successful owned-job stop request precedes the rejected
creation event's first continuation; both original processes reach the requested
cleanup status and the marker is absent. The whole-tree inventory is explicitly
incomplete. These controls validate the observer's admission boundary, not game
startup or protection behavior.

CPU accounting uses the retained original process handles. Initial kernel/user
counters come from the existing authenticated creation-time query before the
first creation-event continuation; their journal emission occurs afterward.
Nominal one-second per-process samples and a final sample after the original
handle wait retain actual API results, errors and QPC bounds. Missing samples
remain unavailable observations. Each series is capped at 128 samples, and CPU
rows remain separate from debug-event continuations.

The owned CPU case records 18 valid samples, including 16 periodic samples and a
terminal sample. Its checked deltas total 203.125 ms of process CPU across a
16.5486-second observation span. These counters do not identify initialization,
useful work, a deadlock or semantic progress.

Unload and process-exit boundaries close image generations separately; process
exit does not invent per-DLL unload notifications. Missing file metadata,
conflicting ranges or cumulative job/debugger process-count differences leave
image mapping incomplete. Debugger receipt QPC values are not ETW event timestamps.
Disk sections and image/RVA matches do not establish live code bytes, a
terminating caller, engine initialization or ordinary startup behavior without
a debugger.

An earlier comparison of unmodified build `26251122` identifies stock `bin/runtime.dll` as a
separate child process image, matching the file ID and SHA-256 of its pinned
input. The same file is also mapped in the root process. The then-current policy
admitted only the main executable's identity and stopped this stock runtime
child before continuing its creation event. Both process exits were the
diagnostic's requested `0xCDD00002` cleanup status. This identified a limit of
the observer's admission policy; it did not identify a natural game failure or
protection rejection. The current policy includes the exact stock runtime
identity as described above.

A 60-second comparison of unmodified build `26251122` admits that exact stock
runtime child and records 79 successful debug-event continuations, two original
process lifetimes, ten terminal thread capabilities and 47 image generations.
Both processes reach their validated initial loader breakpoints; seven other
first-chance exceptions are forwarded, with no second-chance exception recorded.
The capture records no rejected process creation or missing image metadata.

Neither process exits naturally within the observation budget. The observer
stops both with its requested `0xCDD00001` timeout status, drains their debug
exits and waits on the original capabilities. Here `capture_success=true`
describes successful collection and cleanup; `timed_out=true` and
`natural_exit=false` describe the target outcome. No `game.dll`, stock engine
plugin, Party or PlayFab image load appears in this bounded capture. Engine
initialization and the reason for the continued wait remain unverified; the
timeout alone does not establish a game crash, hang or protection rejection.

Each process has 61 valid CPU samples: creation metadata, 59 periodic samples
and a terminal sample, with no unavailable or truncated observations. Across
approximately 60.08 seconds, the main process accumulates 57.5 seconds of process
CPU and the runtime child 0.078125 seconds. This accounting difference does not
identify the executed functions, useful work or the reason for the wait. No
window captions or dialog contents were collected.

Both full-copy passes validate all 168 files against depot SHA-1 hashes and
sizes. SHA-256 hashes for all 1,174 held inputs match before and after capture,
and all 180 copy paths, owners, groups and DACLs are unchanged. Cleanup removes
the temporary profile and restores both saved directory DACLs, with no rule or
loopback exemption matching its exact SID. These diagnostic checks do not
establish the eventual client's user-data, Cloud or official-service isolation.
