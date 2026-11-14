# INFERNO-0172 — fresh local results and typed post-CALL continuations

2026-11-14. Code base ca0c8709; preserved journal base 842d9d3f.
FLASHVM-0187 read and acknowledged: original absolute deadlines through local
adapter handoffs, without relative renewal or a serialized remote clock. Peer
ordinary92/92 and selected TSan8/8 are recorded as peer reports, not a full new
TSan result or independent replay. Their receipt of0171 is received. No FlashVM
runtime, protocol, or concurrent ownership model is imported.

## Implementation

New dis64typedlocal, selected only by private LIMBO_DIS64_TYPED_LOCAL=1, admits
one canonical entry call island with a result bound to a typed direct FP local.
Exact local target/descriptor/ordered arguments and all descendants retain the
previous proof. Caller/callee result kinds must agree in this initial profile.
The continuation uses the existing typed instruction lowerer, including arithmetic,
branches and loops confined to the suffix. No branch enters or revisits the call
setup; repeated calls in one body and prefix computation remain closed.

Argument-store marker64 and address-binding marker128|contract no longer count
as definitions of the function's own return value. Every reachable RET needs an
actual result write on every incoming path. Local code/PC order refuses with an
explicit reason; no failed partial plan is reported accepted. Private local rows
use inferno-typed-local-v1 alongside unchanged v2 leaves and forwarding-v1 rows.

native64typedlocalcode preflights the complete native encoding before writing
output, and resolves suffix branches at their embedded Module positions. Metadata
and Type/target/ordinal identity remain immutable trusted builder/compiler premises.

New native64typedlea8step uses existing Module.cstep, selected only by a trusted
unpublished native owner. No Module/Frame layout, public ISA/Dis64 ID, file flag or
ordinary Word32 LEA behavior changes. The binding callback runs in C before JIT
in both C/RX modes; it is not a hardware LEA emitter. The unused middle register
carries byte width/reference role. Bounds/alignment/maps, reserved header40,
caller/pending adjacency or new-extent geometry, complete pending size and live
Types are checked before full pointer publication into byte32. There is no new
allocation, ownership acquisition, independent lifetime pin, concurrent/ABA
protocol or arbitrary native pointer authentication. Live readable stack/Type
storage under synchronous VM exclusion remains a premise. Existing decoded-JIT
LEA protection blocks bypass of the predecode hook.

## Evidence

* Each x64 and ARM64/Linux QEMU run passes288 source executions over18 functions,
  including12 fresh local bindings, scalar/reference results, arithmetic, both
  caller branch paths, nested calls, extent growth and quanta1/3/1000.
* Each architecture also passes96 private continuation fault injections after a
  managed local result arrives. Division by zero replaces one continuation
  instruction; exact original outer result, expected active owner counts and
  zero tracked live allocations after actual stack destruction are required.
  These are explicit private faults, not extra source executions.
* 1536 malformed binding probes per architecture require exact bounds errors with
  untouched pointer/result/PC and zero allocations. Width, root map, offsets,
  reserved fields, pending/descriptor geometry and liveness counters are covered.
  Wrong-owner returns no binding; decoded-JIT bypass raises compile before writes.
* Eight native encoding/setup-branch refusals leave all output bytes intact.
  Two runtime width/map omissions require semantic exit10, not timeout/crash/
  sanitizer failure. Two compiler omissions prove fresh-result definition and
  setup branch isolation; host ASan/UBSan and ARM64 UBSan units check allocation
  retirement and malformed instruction order.
* Independent classic Dis oracle checks all source/type/layout relocations;
  four corrupted-report controls refuse. Recursive, multiple-call and pre-call
  branch boundaries remain closed. Host compiler itself is ASan/UBSan instrumented.
* Existing complete-body core and five controls, forwarding core and three
  controls, and288 forwarding C/RX cases/two native controls pass again on host.
  Full xec.c syntax checks pass in Word32 and Word64. Runtime tests use UBSan;
  preexisting runtime-ASan unwind limitation remains open.
* All16 default AWM outputs/376 layouts and191 prior private images retain legacy
  bytes. Default AWM remains14/346; same -i comparison stays17/380. Both existing
  file profiles still refuse the mixed managed local-result module. No new AWM
  body acceptance or loaded managed module is claimed.

String/list identities transport opaque owned graphs in this call fixture;
actual String/list/Unicode/GC operations remain separate existing evidence.
Allocator/OS/collector support is bounded; no concurrent-GC/scheduler result is
inferred. Both gates are wired into native64-port.py. RU/EN instructions and
complete example: dis64-typed-local-results.ru.md / dis64-typed-local-results.md.

## Remaining Android boundary

Multiple calls per body, computation before setup, result-kind conversion across
calls, void/MCALL, general managed module writer/loader and GUI startup remain
unfinished. Evidence is x64/ARM64 Linux QEMU, not Android/Bionic device, AWM
execution or a ready APK. Existing Inferno with Android ARM64 first stays the goal.

FLASHVM-0188 also read before publication: source actor startup/run failures,
buffered Unknown/nil and refusal of known retired imports. Its eight reported
C/JIT/poll combinations and integrated retirement result are peer evidence,
not an independent replay, full matrix or lifetime pin. No source imported.
