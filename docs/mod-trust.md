# Community-required mods and player trust

This is the planned admission and execution model. The current launcher discovers
local metadata and saves profiles; it does not download, deploy or run mods yet.
A server requiring a mod is a compatibility requirement, not permission to execute
code on the player's computer.

## Execution classes

| Class | Intended use | Execution boundary |
| --- | --- | --- |
| Validated data | Campaign definitions, supported stats and content recipes | A closed schema with bounded values and known operations; no executable expressions, raw memory writes, arbitrary resource patches or build steps |
| Sandboxed logic | Portable gameplay rules and events | A sandbox runtime with a narrow game API, resource limits and no ambient filesystem, network, process or native-pointer access |
| Native code | Engine hooks and mods needing native access | Separately approved local code, with the permissions of the process that loads it; never presented as sandboxed |
| Unclassified/raw content | Unknown packages, arbitrary binary game resources or legacy formats | Unsupported until reviewed; a manifest claiming to be data does not downgrade its risk |

C++ remains suitable for the launcher and trusted adapter. C/C++ can also compile
to WebAssembly for restricted logic, but existing native DLLs cannot simply be
placed inside a Wasm sandbox. Wasmtime is one candidate, not an adopted dependency.
Its model gives guest code access only to explicitly provided imports; the host
must still implement those imports safely. See the
[Wasmtime security model](https://docs.wasmtime.dev/security.html).

The sandbox must expose stable handles and typed commands, not process addresses,
arbitrary byte writes, FFI, shell commands or unrestricted SDK calls. A trusted
adapter applies validated commands in the game. CPU/time, memory, command rates
and output sizes need bounds. The runtime, adapter and parsers remain trusted
code, so a data or sandbox label is not a claim of zero risk.

## Requirement and approval flow

1. A community or lobby advertises a bounded, versioned profile: community identity,
   game build, adapter version, exact package identities and hashes, required
   capabilities, dependencies and meaningful load order. It may require server-side
   mods without installing corresponding client code when the protocol permits it.
2. The launcher compares the profile with local packages and approvals before
   joining. It shows which requirements are already met, need downloads, need
   native-code approval, or are unsupported. Declining leaves the current profile
   intact and explains why that lobby cannot be joined.
3. Downloads go into a separate staging area with byte/file/expanded-size limits,
   bounded timeouts, safe paths and no links or scripts. Verify bytes and classify
   the actual contents locally; do not trust the server's claimed class.
4. Native code requires explicit local approval of the exact package identity and
   content hash. The approval includes requested capabilities and adapter ABI.
   Changes invalidate approval. An operator-controlled signature or a server's
   assertion that its own mod is safe cannot create this approval. Any future
   opt-in trusted-publisher policy must be distinct from trusting a community.
5. Install a complete compatible profile transactionally at a safe idle point.
   Recheck the staged bytes, deployment paths and approvals immediately before
   activation. Never switch native modules during an active mission. Keep the
   user's usual profile separate and support rollback.
6. Admission compares the selected profile again. A compatibility digest helps
   prevent mismatched sessions; it does not prove an untrusted client is honest or
   constitute anti-cheat attestation.

The trust decision also covers compilation. Downloading C/C++ sources and building
them locally does not make them safe. Source packages must not supply arbitrary
CMake projects, compiler plugins or pre/post-build commands for automatic execution.
A future build service needs a pinned toolchain, a constrained build recipe and a
contained worker. The compiler and produced artifact are part of the trust model.

Hashes identify bytes. Signatures identify their signer. Neither proves benign
behavior. Previously approved mods must not silently authorize different versions,
new capabilities or new dependencies.

## Native loader boundary

HellForge's loader loads DLLs into the game and calls their initialization function.
A native plugin can invoke operating-system APIs without going through the mod SDK.
Native capability declarations describe intent; they do not constrain arbitrary
native code to those capabilities.
Blocking child processes or outbound Internet traffic alone does not prevent it
from reading user files, modifying other writable files or contacting local services.
The current [HellForge review](hellforge-evaluation.md) distinguishes its controls
from a native-code sandbox.

The privileged installer/updater and credential storage must remain outside mod
control. A local bridge must authenticate the specific client session and expose
bounded game operations, not arbitrary HTTP forwarding or filesystem access.
Whole-process isolation is additional protection to verify, not a substitute for
native-code consent. No community can relax the player's local policy remotely.

## Verification required before enabling automatic mod setup

- Reject a native package relabeled as data, a Lua/raw patch hidden in a content
  archive, malformed schemas, unknown commands and undeclared executable files.
- Reject changed bytes under a previously approved ID/version, changed capabilities,
  unapproved dependencies, archive traversal, links and decompression bombs.
- Prove that a server-provided signer or trust flag cannot approve execution.
- Verify safe cancellation, interrupted downloads, rollback and unchanged ordinary
  profiles. Recheck after restart and immediately before loading.
- Test sandbox CPU/memory exhaustion and attempts to access files, sockets, native
  memory and processes, including misuse of allowed host functions.
- Test two clients with differing profiles, refusal, consent, restart and rejoin;
  distinguish synthetic contract checks from real multiplayer verification.
