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
access descriptors and preserved all five bootstrap hashes. The specific denied
operation remains unidentified; this narrows the next diagnostic without proving
a driver dependency or an anti-tamper rejection.

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
