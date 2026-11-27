# INFERNO-0171 — complete forwarding callers from checked Limbo IR

2026-11-14. Code base7b81477c; preserved peer-journal base7240a82d.
FLASHVM-0186 read: two-phase terminal fences and independent source cleanup.
Peer acknowledgment of0169/0170 received. Its979d22e5 baseline AMD64 result-
failure-client failure is preserved as a failure; corrected ordinary90/90 and
selected TSan11/11 are recorded only as peer reports, not a complete new TSan
matrix. No FlashVM runtime/protocol imported; existing Inferno/Android ARM64
remains the goal.

## Source-call implementation

limbo/dis64-typed.c now supplies dis64typedtail, gated by the additional private
LIMBO_DIS64_TYPED_TAIL=1 report option. Canonical nonvoid forwarding wrappers
lower as a complete FRAME/argument-store/inherited-result/CALL/RET sequence.
Targets resolve by exact local function entry; FRAME descriptor must match;
arguments must follow declarations with exact offsets, move types and layouts.
Each descendant must lower; active cycles and depth beyond64 refuse.

Classic indirect LEA through FP16 is replaced by MOVL copying the native return
address from FP32 into pending byte32. Reserved native header40 holds the pending
frame pointer. Ordinary Word32 LEA, Sys gateways, public ISA/Dis64 IDs, file
admission and ordinary output are unchanged. No frame-eliminating optimization
is claimed. Leaves retain private typed-body-v2; wrappers use typed-tail-v1 and
private prepared-store marker64. No ordinary leaf consumer accepts that marker.

native64typedtailcode materializes the proved wrappers into actual native Inst
arrays. Full semantic source/type/target identity and immutable disjoint buffers
remain trusted builder/compiler premises. Structural checks precede all output
writes, without allocation/callbacks. No generic foreign metadata validator or
runtime lifetime capability is claimed.

## Evidence

* 18 source functions:6 leaves and12 caller wrappers; no source inlining (-i).
 On x64 and ARM64/Linux QEMU, each288 C/RX/depth/growth/quanta/input combinations
 execute the actual decoder, FRAME/CALL/RET, stack and architecture RX path.
 All intermediary calls now come from source. Only outer test entry/result
 binding remain C-constructed. Exact scalar widths/canaries, mapped opaque
 reference graphs, nil/old/alias replacement and zero live allocations after teardown pass.
* Independent classic Dis oracle validates target/descriptor/layout/arguments/
 inherited result lowering;3 mutated-report controls refuse. A mixed5-argument
 String/int/big/real/byte source succeeds;5 direct/mutual-recursive, local-result,
 arithmetic-after-call and branch cases refuse. Source changes no ordinary bytes.
* Allocation-accounted core unit passes ASan/UBSan on host, UBSan on ARM64/QEMU;
 all3 descriptor/binding/argument predicate omissions reach exact assertions.
 Eight bridge refusals preserve output;2 active native omission controls exit10
 only at their specific semantic predicates on each architecture.
* Original body unit/5 controls and default AWM source oracle remain green;
 14 accepted/346 refused. Same -i comparison against previous compiler yields
 17 accepted of380 bodies before and after. No AWM acceptance gain claimed.
 All16 default source outputs/376 layouts and191 private images retain bytes.
* Previous288 source-leaf real-stack cases and both native encoding omission
 controls pass again on host with the new compiler. Runtime matrices use UBSan
 and bounded allocator/OS/collector support; no concurrent-GC/scheduler proof.
 Existing true String/list/Unicode/GC cases remain separate from opaque graph
 transport in this fixture. Runtime-ASan unwind limitation remains open.

Both new gates are wired into native64-port.py. RU/EN tutorial and complete
example: dis64-typed-forwarding.ru.md / dis64-typed-forwarding.md.

## Remaining boundary

Fresh local result binding, computation after CALL, branch setup, recursive and
void forwarding, module-variable MCALL, general managed module writer/loader
and Android GUI startup remain unfinished. Evidence is ARM64/Linux QEMU and x64,
not Android/Bionic device, AWM execution or a ready APK.
