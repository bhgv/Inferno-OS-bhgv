# Series0082: real VM result addresses and terminal call cleanup

## Implemented boundary

A new optional trusted `native64lea8step()` uses existing Module.cstep execution
identity and xec-before-JIT priority to bind a complete eight-byte result address.
It checks the actual caller frame/Type/tail and prepared pointer addressing shape.
Ordinary LEA, wire flags, emitters, JIT cache code and file readers are unchanged.
No file constructor selects this callback. It is a runtime prototype, not new
shared file-call admission.

[Detailed execution contract, limits and reproduction](dis64-native-result-calls.md).

Verified immutable raw caller/leaf sections now execute through real FRAME/CALL/
RET, native C/RX dispatch and production stack/heap/error cleanup. The trusted
fixture reuses actual direct instruction lowering and validates indirect fields.
It is not the production loader or a whole-container golden. WORD32 wire int32
and native pointers64 are used; WORD64 probes cover only legacy/address isolation.

## Local evidence

10080 actual one/two-call runs cover int/bool/big/real results, temp40/48/56,
frame padding, normal/extended stack, seven quanta, current/saved REG cleanup,
early/late division errors and first/second FRAME allocation failure. Earlier
copied results survive later failure. A physical tail write before error never
executes the caller's later result copy.96 malformed raw vectors refuse BEFORE
native allocation.96 width/cache/extent probes switch ordinary/trusted execution
on the same validated cached module across WORD32/64.528 shape/fallback probes
check error ordering, no stores/allocations and inactive/non-ILEA fallback.

Four active source controls require seven semantic failure executions: trusted
WORD truncation, lost cstep priority, widened ordinary C LEA and early shape
check removal. The ordinary C-body control is C-only; other controls run C/RX.
Default optimized nonrecovering UBSan passes. Existing local VM-address checks
also pass both WORD models; no ordinary handler was changed.

Native ARM64/x64 and RV64/QEMU validation is pending at this source checkpoint.
The prior3338/29,1548/26,611/14,2029/13 and older gates remain in the port driver.
No target execution is inferred from local runs or prior series.

FLASHVM-0070 agrees with private0080/0081 contracts and independently replays
1548/all26 and3338/all29 under UBSan. These are peer runs, separate from our
native matrix. Its hosted elastic executors are not imported into Inferno.
The cstep execution-mode proposal and its dispatch limits are sent for review.

## Remaining and coverage

Debugger/coverage paths bypassing cstep, direct leaf-entry invocation, AOT policy,
caught/resumed handler result state, general caller CFG/non-leaf DAGs, production
container/proof binding and out-of-line compiler output remain. Shared whole-file
C/RX goldens and explicit peer activation agreement still precede enabling calls.
This is a Linux scalar/runtime checkpoint, not Android APK or full managed GUI.

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

Counts remain unchanged; the trusted LEA8 step intentionally uses checked C.
Production new-format file admission remains43/173 (24.86%), a separate metric
from old opcode-entry JIT coverage and full application/runtime readiness.
