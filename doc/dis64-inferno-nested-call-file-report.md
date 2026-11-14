# INFERNO-0178 — source/file nested scalar call DAGs

2026-11-14. Code base97c7ce00; preserved peer journalf4456bb2.
FLASHVM-0195 read/retained. Owner-produced copied call-chain snapshots, bounded
stack traversal, frozen requester tickets and the public residentstack reader
remain peer evidence with their owner/lifetime/boundedness limits. Reported
11/11 ordinary and11/11 TSan selections are not a new full architecture run here;
59 is the listed architecture set, the newest CI was pending and the earlier CI
cancelled. No peer implementation or vendor-pin change is imported.

## Implementation

The existing limbo -64c source producer and explicit -X dis64-calls loader now
accept linear helpers that call other helpers. Call-free leaves retain their
checked CFG/loop/return subset. Shared callee DAGs, several depths, fresh results,
forwarded return expressions, void calls and int-to-Big/Real conversions in
intermediate callers are supported. Recursive graphs and branches in a body with
calls remain refused. Scalar kinds stay void/int/big/real; managed data/imports,
byte signatures, exceptions and arbitrary file dependencies are outside this path.

The writer emits canonical result binding to the current frame's own final8-byte
tail at every depth. A return-expression call no longer forwards its incoming
FP32 result pointer: it copies the completed local tail through scratch to that
pointer. Instruction plans include these extra copies; function/CALL/leaf branch
relocations retain their actual compiled coordinates. All compiled functions
still need declared exports. Existing256 instruction/64 function/64 parameter/
1MiB frame and image limits remain.

New dis64-call-chain.h derives graph edges from raw CALL targets and proves each
function once, bottom-up. No progress means a self/mutual cycle and refusal.
Canonical FRAME identity, argument geometry/kind/completeness/initialization,
result binding/readiness, incoming return ownership and return coverage are
checked before publication. Every proof step is charged to the existing budget.
The old root/leaf checker is attempted first, retaining its exact accepted proof
summaries. Only the extension infers outgoing result tails for nonroot callers.

Publication of records/count/image remains atomic on semantic failure. Work and
scratch are disposable and can change. The compiler checks the complete encoded
image before Bwrite; the runtime recomputes proof from immutable file bytes before
constructing executable owned descriptors. C manifests never supply file authority.
Startup/cache/admission/default-profile rules are unchanged. No public section,
feature, signature, opcode or format ID changes: Dis64v1.0 ABI1 and the existing
five sections/scalar signaturev1 are used.

## Verification

Twenty-eight whole source programs have deterministic -64c/-64c -i bytes. All28
classic outputs match the previous compiler using identical source paths/options.
An independent Python parser checks sections/code/signatures/layouts. Its new
linear value-state and DFS graph model checks callers; unchanged call-free leaves
use the previous CFG/return oracle after erasing only independently checked caller
bodies. Both self/mutual cycle mutations and the other deep malformed images are
independently rejected too.

Each x64 and ARM64/Linux QEMU runtime gate executes all28 through real readmod,
parsemod, cache/admission, scheduler, C/RX and teardown. Exact value bits are
checked. Every read/parse/construct allocation is failed in turn; owned modules,
pins and file handles retire. Eighteen damaged images refuse: previous eight plus
ten deep mutations including self/mutual recursion, frame/result/return owners,
wrong result source, missing caller/leaf stores, early RET and callee EXIT.
Ten unsupported source programs emit no payload; the former nested-call refusal
has become a recursion refusal. Ordinary/default launch remains closed.

Both rebuilt full classic-mk console emulators execute56 real OS roots each:
48 normal and eight zero-divide retirements, C/RX. They cover actual startup,
worker, return/error unwinding and final retirement. Numerical results are checked
by the separate bounded runtime gate. Full console uses normal target flags;
file runtime uses UBSan. Existing bounded GC/OS seams and runtime-ASan unwind
limitations remain explicitly scoped.

The separate raw checker runs94 positive/negative/budget/capacity cases on x64
ASan/UBSan and ARM64/QEMU UBSan. Old-profile proof records/image summaries compare
exactly. All refusal outputs/count/image retain canaries. Six active omissions
must fail with semantic exit10: self-cycle, mutual-cycle, partial publication,
leaf return coverage, result-binding owner and incoming-return owner. Controls
never execute malformed modules. The writer IR unit and its three final-proof/
binding/cleanup controls also pass host ASan/UBSan and ARM64/QEMU UBSan.

Previous inference9-image/30-refusal/312-runtime corpus and four controls pass;
3343 old bound cases retain389 accepts/2954 refusals. The compiler bootstrap is
rebuilt with host ASan/UBSan from current headers. Classic mk includes the new
header dependency, and native64-port.py includes the raw DAG checker alongside
the expanded writer/runtime/boot gates. RU/EN examples and contracts are updated.

## Remaining Android/client boundary

This closes nested scalar source-to-file execution on x64/ARM64 Linux QEMU. It
is not the general managed/import graph writer/loader, dependency/cache/namespace
ABI admission or heap-operation implementation required for AWM. Android/Bionic
GUI startup and an installable working APK remain unverified; no NDK/device run
is available in this environment. The trusted owned-import builder remains a
separate existing C path. Managed wire expansion still requires coordination;
no new managed IDs or FlashVM VM implementation are introduced here.
