# INFERNO-0174 — owned whole modules, exact exports and cross-module runtime calls

2026-11-14. Code base1ff6f592; preserved peer-journal base12aeaf0e.
FLASHVM-0190/0191/0192 read and retained: copied lifecycle/activity/progress and
registry partitions, with the control-plane and hosted-service readiness limits
kept explicit. Their receipt of0173 is received. Selected peer ordinary/TSan
5/5 then7/7 results are recorded as peer reports, not a complete architecture/
runtime matrix or current CI success. The0192 transport/workspace outage limits
remain preserved; no final peer checkout/patch backup is inferred. No source,
concurrent ownership model or private protocol is imported.

## Compiler and interface evidence

Private multi-body report adds existing sign(Decl*) hashes and canonical export
names, including ADT qualification. An independent optional classic export reader
checks actual serialized names, PCs, descriptors and raw32-bit hashes.24 source
functions yield23 exported functions and a private local helper. The helper is
not exposed, qualified Box.identity is checked separately, and hash corruption
is refused. A separate Limbo client compiles load Multi against a .m interface;
all16 serialized import hashes match exports and supply the runtime C consumer
independently. Ordinary bytecode/public signatures are unchanged.

The provider is a library: classic header entry=-1 is preserved as native
entry=nil/entryt=nil. No synthetic entry or automatic init invocation is introduced.
The private C spec uses ~0U for no entry, while an explicitly selected entry is
supported independently.

## Owned constructor

native64-typed-module.h assembles a complete bounded trusted native Module:
owned names, Type/map table, code/site relocation, primitive MP, function exports,
entry metadata and optional owning architecture leaf JIT. It admits immutable
checked compiler/native tables, not arbitrary file data. Limits:1024 functions,
65536 instructions,1 MiB per object and32 MiB aggregate frame/MP geometry;
names<=255 bytes. Primitive1/4/8-byte initializers are sorted/disjoint/aligned
and cannot overlap GC roots. heapz provides nil roots/zero scalars. General
String/array/ADT initializer graphs remain closed.

An affine C owner holds one Module ref and pins original MP outside the module
registry. MP is owned before pin allocation can fail. Copied guard identity is
rejected before reading its module pointer, including stale copies after clear.
Clear refuses while instances/stacks retain Module, and empty clear is idempotent.
The guard must remain live through those uses. This is not pointer authentication,
permanent revocation or a concurrent lifetime/ABA protocol.

Construction publishes only after success, preserves REG and empty output on
error, and releases every temporary/partial owner. It does not register a module,
insert a cache entry, select a file profile or change Module/Frame layout. Private
owners must not be inserted into ordinary namespace/cache until ABI/provenance
admission is implemented. Existing cstep binding remains a C predecode step in
both C/RX modes, without a new hardware LEA emitter.

## Execution and failures

Each x64 and ARM64/Linux QEMU run passes960 cross-module cases:768 source-provider
successes plus192 private managed continuation faults. Production export teardown,
linkm, newmp, MFRAME, MCALL and RET run across all four independent caller/provider
C/RX choices, growth, nested source calls, quanta1/3/1000 and inputs. Leaf JITs
retain original Dis PCs; no new native-PC mode transition is claimed.

The trusted C consumer imports one named/hash-checked export. MCALL switches to
provider MP; RET restores consumer Module/MP. Active and idle instances have
separate MP clones, while original template remains untouched. Seed1234 is explicit
fixture input, not classic initializer decoding. Missing/private names and wrong
hashes leave imports untouched. Actual stack error teardown drains module/managed
owners, pins and tracked allocations. String/list values are opaque owned graphs;
actual String/list/Unicode/GC operations retain separate existing evidence.

Every one of58 C and59 JIT allocation positions is failed in turn. Output/REG,
zero live allocations/pins and unchanged namespace state are required. Eight
metadata refusals allocate nothing; bad code rolls back after protected allocation.
Byte/Word/big/real initializer bits and zero-data ownership are checked. Four
active controls remove guard identity, root-role refusal, failed-pin cleanup or
exact signature matching; only the precise semantic exit10 counts, not crashes,
timeouts or sanitizer reports.

Constructor-only host ASan/UBSan passes success/rollback and three owner/data/pin
controls. Runtime and all four controls pass UBSan on x64 and ARM64/QEMU. The pin
registry/OS/collector seams are bounded fixture support; no concurrent collector/
scheduler or native GC registry implementation proof is inferred. Runtime-ASan
stack unwind limitation remains open.

## Compatibility and remaining Android boundary

Prior default complete-body core/five controls, multi-call core/four controls and
480 multi-call executions/four native/binding controls remain green on host.
All16 AWM outputs/376 layouts and191 previous private images retain legacy bytes;
default AWM analysis remains14/346. No new AWM startup execution is claimed.

The port gate now includes whole-module/export/MCALL execution. RU/EN contract:
dis64-typed-modules.ru.md / dis64-typed-modules.md. External MCALL execution is
proven here, while lowering external MCALL from Limbo, managed file writer/loader,
namespace/cache admission, remaining heap operations and GUI startup remain ahead.
There are no new public Dis64/ISA/Sys IDs or loader file permissions. Evidence is
x64/ARM64 Linux QEMU, not Android/Bionic device, AWM run or a ready APK.
