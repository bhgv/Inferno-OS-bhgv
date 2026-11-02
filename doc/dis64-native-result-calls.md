# Trusted native result-address bridge: checkpoint0082

Status: owning-source runtime prototype on the authoritative `xec` path. File
CALL/LEA admission remains closed. No production file reader assigns this mode.

## Explicit execution identity

`native64lea8step()` in `libinterp/xec.c` is an optional trusted `Module.cstep`.
It handles ILEA only when the active module explicitly owns that SAME callback.
Other opcodes return zero without decoding; another callback/nil is not retagged.
The existing xec order invokes cstep before ordinary native fast/complete entries.
Ordinary `OP(lea)`, opcode IDs, rt/wire flags, emitter layouts and JIT tables are
unchanged. The callback writes a complete native eight-byte pointer in this mode.

The callback requires exact AFP source and indirect AFP destination, AXNON,
prepared-pointer prefix40/48/56 and secondary32. It resolves the current frame's
real Type, including Stkext.TR when Frame.t is nil, requires np0/aligned size>=72
and the exact final caller result-cell offset. Shape checks precede vmdecode;
postdecode source identity and the full writable eight-byte destination span
are checked again before memmove. There is no allocation or ownership transfer
inside this step. Incoming prepared-frame provenance remains the checked
constructor's obligation, not a general pointer capability accepted from a file.

The explicit callback is the prototype's execution-mode identity. Cached ordinary
WORD-sized LEA code stays ordinary; the trusted callback intercepts the instruction
before it can use that entry. The fixture switches nil/callback/nil/callback on
one module while compile() validates and REUSES the same original JIT mapping.
This does not alter or introduce a proof cache. It is a proposal/evidence boundary
for FlashVM review, not a new executable shared profile.

IMPORTANT remaining boundary: direct arbitrary leaf/complete-entry invocations
and debugger/coverage dispatch paths that bypass cstep are NOT this mode's entry
contract. They must be audited/integrated before production file activation.
Coexistence with other AOT callbacks also needs an explicit constructor policy.
This prototype does not silently replace another module's existing cstep.
Mode assignment in the fixture is before first publication or between completed
stack retirements, with a single owning thread and no active VM turn. FLASHVM-0072
agrees with this trusted scope and independently replays the owning tests; it
does not authorize shared file activation. Future mode set/reset must be quiescent
under the owning scheduler/heap exclusion; no concurrent unsynchronized mutation.
A production constructor must reject incompatible pre-existing cstep or define
audited composition. Function identity alone never proves file/container/local-
declaration provenance.

## Actual raw-proof to VM execution

`utils/tests/native64-result-calls.py/.c` feeds independently encoded raw code,
exports/pool/types and compiler-style local declarations to0081's full preflight.
Rejected inputs allocate no native Module/stack. Only accepted immutable facts
are lowered by the trusted fixture: production direct instruction lowering is
reused, with bounded ushort indirect destination fields installed afterward.
Production FRAME, decoder, CALL, MOV/CVTWF, RET, stack growth/unwind, heap and
native emitters are linked unchanged. This is NOT the current file loader and
not a whole Dis64 container/directory acceptance golden.

The wire integer model is WORD32 with native pointers64. Positive int32/bool4,
big/real8 arguments/results and one/two-call chains execute under C and native
RX dispatch. Both root copies and result tail carry guard bytes: four-byte
stores cannot overwrite the high four bytes. Address bytes are checked BEFORE
a bad/truncated pointer can reach CALL. Distinct first/second values prove that
earlier copied locals survive a later failure and are not replaced by the new
physical tail contents.

Failure cases are terminal/unhandled: division before result store, division
AFTER a physical result store but before RET, allocation refusal at first or
second prepared FRAME. A late store may change the raw tail, but the failed
call never executes the later caller result copy. The runtime gains no new
readiness flag here; readiness is observed through normal continuation versus
actual protected failure transfer. Recovery/resumed-handler result semantics
remain separate future work. Every run destroys real current or saved REG
storage and requires no live tracked allocation/module owner after unload.

## Matrix and controls

144 accepted section vectors (72 single-call and72 chained) run over interpreter/
RX, normal/extended stack, seven post-binding quanta and current/saved REG cleanup.
First/second FRAME allocation failures add cases:10080 actual runs.96 malformed
shape/binding/return vectors refuse before native allocation. These refused
inputs are not executed or counted as successful VM runs.

A separate native WORD32/WORD64 width fixture supplies48 ordinary/trusted C/RX/
cache/extent probes per model,96 total. It has no CALL wire execution in WORD64;
WORD64 width probes do not claim Dis64 int32 semantics under legacy WORD64.
Each model also executes264 shape/fallback probes,528 total: bad source mode,
temp/displacement/tail, small/unaligned/managed caller Type, nil/H FP, inactive
callback and non-ILEA fallback. Invalid shapes must leave PC/result bytes and
allocation counters unchanged before decoder publication.

Four isolated source controls require semantic status10: narrow the trusted
write to WORD, bypass xec cstep priority, widen ordinary C LEA, and remove the
early addressing-mode check. Width/priority/mode controls run C and RX; ordinary
C-body control runs C only because native ordinary fast code is intentionally
unchanged. Seven control executions per target, no compiler/signal/sanitizer
failure accepted as detection. The mode control preserves safe backing and
stops on predecode ordering, not an invalid native CALL.

Run `python3 utils/tests/native64-result-calls.py`; native port configuration
selects ARM64/x64/RV64 compiler/runner. Default local flags are optimized,
NDEBUG and nonrecovering UBSan. The independent section oracle supplies expected
acceptance; expected scalar bits are independently encoded, not read from VM
output. No Java, worker/file adapter or complex handler replacement is involved.

[Target report](native64-series0082.md). General caller CFG/non-leaf graphs,
production proof-to-container binding, out-of-line compiler output, instrumented
execution, shared whole-file C/RX refusal/error goldens and peer mode agreement
remain before executable admission.
