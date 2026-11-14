# Owned native64 modules and real export linking

[Русская версия](dis64-typed-modules.ru.md).

The [multiple-call gate](dis64-typed-multiple-calls.md) supplied each runtime
Module manually. `include/native64-typed-module.h` now constructs a complete
bounded native module from trusted compiler-derived code/layout/site metadata:
owned Type descriptors, one program, local call relocations, MP storage, exports,
entry metadata and optional architecture leaf JIT.

## Run

```sh
python3 utils/tests/native64-typed-module.py --limbo /path/to/host/limbo
```

For ARM64/Linux QEMU use the existing CC/NATIVE64_ARCH/NATIVE64_RUNNER variables.
The port driver includes this gate. Constructor-only host sanitizer execution:

```sh
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
python3 utils/tests/native64-typed-module.py --limbo /path/to/host/limbo --construct-only
```

This mode constructs/compiles code and tests rollback without executing VM stack
error paths. Runtime execution uses UBSan; the existing runtime-ASan unwind issue
remains open. LeakSanitizer is disabled in this host environment, while explicit
allocation/pin accounting is required on every tested constructor path.

## Actual interface signatures

With the private multi-body report selected, Limbo now supplies `signature` via
its existing `sign(Decl*)` and canonical `export_name`, including ADT-qualified
names such as Box.identity. These are the existing32-bit Limbo interface hashes,
not newly assigned structural Dis64 signature IDs.

The independent classic reader's optional export mode reads the original module
name and every serialized export PC, descriptor, name and raw hash. The module
gate requires exact agreement with compiler rows. Its source has24 functions,
23 declared exports and one private helper: the helper remains callable locally
without becoming externally visible. Qualified ADT method names are checked in
a separate source case. A separate Limbo client compiles `load Multi` against
a `.m` interface: all16 serialized import hashes match the provider exports.
The runtime C consumer uses those independent import hashes. Corrupting a
reported export hash is refused by the oracle.

## Constructor and owner

`Native64ModuleSpec` describes immutable readable native tables. Functions contain
names/hashes/export visibility, typed code, frame geometry/maps and local sites
whose targets are function indices. Shared MP metadata supplies the descriptor
and sorted nonoverlapping scalar initializers. Byte/Word/eight-byte bit payloads
cannot overlap mapped roots; heapz provides zero scalars and nil managed cells.
String/array/ADT initializer graphs are not implemented by this API.

Preflight bounds names, total code, frame/data geometry, maps, initializer roles,
visibility and local target indices before allocation. Construction copies names,
maps, code and payloads, resolves every local target, creates a sentinel-terminated
export table and selects the trusted local-binding cstep. Structural instruction
errors during materialization are protected failures with complete rollback.

```c
Native64ModuleOwner owner = {0};
if(!native64typedmodule(&spec, wantjit, &owner))
    return 0;
/* owner.module is owned and unregistered; keep owner at this address.
 * Create/use trusted native instances, then retire all of them. */
if(!native64typedmoduleclear(&owner))
    return 0; /* Instances/stacks still retain this module. */
```

The owner retains one Module reference and pins its original MP while the module
is outside the namespace registry. Construction adopts MP before fallible pin
allocation, so a pin failure still has a cleanup owner. The guard is noncopyable:
its identity is checked before consulting a copied module pointer, including
copies left after the original is cleared. Clear is idempotent when empty and
refuses while other module references exist. Keep the guard alive through all
instances and stack uses. This trusted C identity is not pointer authentication,
permanent revocation or a concurrent lifetime/ABA protocol.

Success returns one unregistered module; failure preserves the empty output and
REG. Temporary workspaces and partial exports/Types/code/MP/JIT are retired.
There is no namespace/cache insertion or file-loader selection. Do not enroll
these private owners in the ordinary module cache: admission/provenance and ABI
compatibility are separate unfinished work.

## Cross-module execution

The fixture uses production export teardown, linkm name/hash resolution, newmp
cloning, MFRAME, MCALL and RET. Its outer consumer remains a trusted C program;
the provider's complete functions/local calls are compiler-derived. Consumer
and provider independently select C or RX dispatch. Both retain ordinary Dis PCs.
This is equal compiled-mode local/remote-Module dispatch with independent leaf
JITs, not a new native-PC mode-transition contract.

The consumer resolves an export into its import slot, MFRAME allocates the actual
export frame, and the existing cstep binds a full-width result address. MCALL
switches Module/MP; RET restores the consumer and releases the departed link.
Two provider instances use separate MP clones. A private scalar seed1234 in the
original template/idle clone makes accidental writes visible; source void calls
modify only the active clone. This seed is fixture input, not serialization of
all classic initializers. Missing names, hidden helpers and wrong hashes leave
import slots untouched.

## Evidence and remaining boundary

Each x64 and ARM64/Linux QEMU run passes960 cross-module cases:768 ordinary and
192 private managed continuation faults, across independent caller/provider C/RX,
growth, nested source calls, quanta and inputs. Results, canaries, current MP,
original/idle MP, exact module lifetimes and full allocation/pin teardown are
checked. String/list identity values remain opaque owned graphs in this fixture;
actual String/list/Unicode/GC operations have their separate existing gate.

Every one of58 C and59 JIT constructor allocation positions is failed in turn,
including pin/JIT stages. Eight metadata refusals allocate nothing; bad code
rolls back after protected allocation. Primitive initializer bits and empty MP
are also covered. Four active controls remove copied-owner rejection, root-role
initializer refusal, failed-pin cleanup or exact signature matching; each must
reach its precise semantic diagnostic, not crash/timeout/sanitizer failure.
Constructor-only host ASan/UBSan and ARM64 runtime UBSan pass. Pin/OS/collector
boundaries use bounded fixture support, not a concurrent-collector proof.

The source external-MCALL lowerer and managed image writer/loader are still
unfinished. This C owner API does not open public Dis64 file admission or change
ISA/Sys IDs, Frame/Module layouts, ordinary bytecode or existing LEA behavior.
Default AWM remains14/346;16 source outputs/376 layouts and191 prior private
images retain legacy bytes. No Android/Bionic device, AWM run or GUI APK is claimed.

A library without an entry preserves entry=nil/entryt=nil. The constructor
does not synthesize or invoke init. Its private C entry sentinel ~0U maps
the classic header entry -1; invoking a command remains the caller's decision.
