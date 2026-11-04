# Non-executable Dis64 storage ownership and actual collector tests

`prepare64storage(image,bytes)` returns an opaque `Dis64StorageOwner`, distinct
from Module/Modlink. The serialized VM caller retains ordinary VM exclusion and
calls `free64storage` once when done. This explicit API changes no file admission,
namespace/cache state, scheduler, instructions, exports, dependencies or JIT.

It first checks the complete immutable storage image, native WORD32/pointer64/
IEEE model and explicit private limits. Every descriptor, including unused mapped
types, is copied into an actual native Type using dtype. Partial nil array entries
permit one common cleanup path. Nonzero MP uses its exact descriptor with actual
heapz; its owner is adopted before fallible ptradd. The successful lease keeps one
external root until release. Present scalar DATA is applied with the exact runtime
H bytes; absent DATA keeps heapz's private zero/nil staging, not a newly agreed
executable wire default. No source or policy pointers survive in the opaque lease.
The staged types do not grant frame/header-root correctness or execution rights.

All source/result/policy storage must be independently live/mapped and disjoint;
source remains immutable until preparation returns. The borrowed structural view
helper requires that lifetime through all uses, while successful owning preparation
copies what it retains. ptradd's native metadata allocation does not perform a VM
collector poll; construction relies on ordinary existing VM exclusion, not a new
concurrent-mutator protocol. No FlashVM heap/root implementation is transplanted.

Run `python3 utils/tests/dis64-storage-owner.py`. Independent raw fixtures from
series0125 drive489 rows:29 successful preparations,690 semantic/injected refusals,
192 actual collector epochs and141,707 active checks per WORD32/WORD32-file model.
The690 includes460 structural refusals and230 individually injected allocation
refusals. A legacy-model build refuses a valid managed image before allocation.

Actual production dtype/heapz/initmem/freeptrs/freeheap/destroy/freetype, complete
gc.c including ptradd/ptrdel/markheap/rootset/rungc and emu kprotect execute. Only
native allocation/pool geometry, formatting and the platform jump are bounded
services. Every managed object comes from actual heapz; no fabricated Heap/Type
is supplied to the collector. rungc(nil) traces only genuine external roots; no
scheduler or Module root is manufactured. Native owner/Type/root allocations have
independent guard bytes and exact retirement ledgers. Cached ptradd metadata is
drained separately, without disabling ordinary hosted leak detection.

Every successful constructor allocation is injected to fail, including owner,
array, each copied Type, managed data and external-root metadata. Protected errors
and partial owners restore native/heap/root baselines. Source bytes are poisoned
after successful preparation, before GC. Wire maps/sizes equal copied native Type
maps/sizes. Runtime nil and initialized MP bits are checked independently. Actual
collector epochs preserve MP and its mapped cyclic child; owner release retires MP
by RC and later GC reclaims the child cycle. An independently retained alias root
survives lease release and keeps that same graph alive until its own release.

Six omission controls (`map`, `pin`, `release`, `types`, `data`, `preflight`) must
exit10 at their exact named semantic predicate. A deliberate failure releases only
fixture-owned native allocations before exit; sanitizer/compiler/crash/timeout or
unrelated failure is never a control pass. Hosted native ARM64/x64 use O3/NDEBUG
ASan+UBSan with ordinary leak detection enabled; local runs use nonrecovering UBSan.

This is a bounded actual GC/storage construction acceptance, not a whole production
pool/scheduler/renderer/device/Android test. Arbitrary Type destructors, managed
instruction execution/import resolution, frame/result roots and concurrent collector
protocols remain outside this API and fixture. Neither the default43-op admission
nor the immutable explicit dis64-calls profile is widened.
