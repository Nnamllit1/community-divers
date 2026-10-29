# Current-client startup investigation

The current client adapter is unfinished. This page records the bootstrap
observed for Steam build **26088801** on 27-28 October 2026 and the limits of the
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

## Pre-exit file-operation candidate

The periodic sampler now retains bounded live instruction bytes, stack bytes,
candidate argument structures and debug registers at its existing sample points.
Ten authored cases pass, including 38 memory-reader checks, the previous 34
ownership checks, a real open of an authored file and a seven-second wait in the
confined fixture. The controls preserve counted binary data, embedded zeros and
UTF-16 code units; they do not treat every object-name buffer as a pathname.

A further retail comparison again exited with `0xC0000244`, with all 43 event
continuations successful and cleanup verified. One pre-exit sample at runtime
RVA `5B3C28` contains a live `SYSCALL` immediately before the saved instruction
pointer. Its accumulator matches the measured system `NtOpenFile` service value;
candidate arguments include share mode `1`, open options `0x2020`, a nonzero root
handle and an eight-byte counted object-name buffer. These fields are consistent
with a file-open-by-ID request. With `FILE_OPEN_BY_FILE_ID`, that buffer can hold
a binary file reference rather than a Unicode pathname; see Microsoft's
[file-open options](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-zwcreatefile).

This is a contract candidate from a periodic snapshot, not an observed API entry
and return. The root handle's volume and target file remain unidentified. Saved
output-slot bytes do not establish initialized or final completion results.
The ordinary termination observers were still armed at this sample, but their
state at exit remains unobserved. The file operation's relationship to the later
exit, protected-image preparation and engine startup remains unverified.
