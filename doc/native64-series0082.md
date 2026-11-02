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

Source: `1acc327083d0b12c1b4f094fd6e6be2ecfd0c58b` on `experiment`.
Main matrix: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41507442205
Additional UTF address ASan/UBSan: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41507442201

The additional native ARM64/x64 UTF sanitizer jobs pass. That is the focused
UTF oracle, not ASan execution of the new result callback or the whole VM.
All three main jobs pass: native Linux ARM64/x64 and RV64/QEMU. Each target
passes10080 verified raw runtime runs,96 pre-allocation refusals,96 width/cache
probes,528 shape/fallback probes and all seven active control executions.
x64 C fixtures use optimized nonrecovering UBSan. Prior3338/29,1548/26,611/14,
2029/13 and older gates pass. Every console ZIP was downloaded and its SHA256
matches GitHub metadata; consoles are ELF64 machines183/62/243. ARM/RV archives
contain178 logs each; x64177. These are separate target executions, not duplicate
counts of FlashVM's owning-source replay.

Older17544 operand,4005 CFG,98032 state,1540 flow and628 catalog cases remain
green. Logs retain241 helper sources/482 whole C/JIT processes,21504 fresh-message,
5376 caught-handler and896 terminal-call cases per target. Six packaged x64
loop/case/procedures JIT0/1 replays report expected module, empty exception and
empty scheduler after retirement. Those remain the existing restricted scalar
images; they do not activate new-format calls in the console loader.

FLASHVM-0070 agrees with private0080/0081 contracts and independently replays
1548/all26 and3338/all29 under UBSan. These are peer runs, separate from our
native matrix. Its hosted elastic executors are not imported into Inferno.
FLASHVM-0072 independently replays the0082 runtime gate and all seven controls,
and explicitly agrees with cstep identity/priority as a TRUSTED prototype only.
Its run is separate from our native matrix. No activation/cache/provenance
capability is granted. Mode changes must be quiescent under the owning scheduler/
heap exclusion, incompatible existing callbacks must not be overwritten, and
all other dispatch paths need enforce/refuse behavior before production use.

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

## Downloads and archive verification

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41507442205/artifacts/13172452732) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41507442205/artifacts/13172074052) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41507442205/artifacts/13171913262) |

| Target | Console ZIP SHA256 |
|---|---|
| ARM64 | `5af2f673aadfc21fbd56de410dd2ef3a605378ff18d883553e9572556f73fa93` |
| x64 | `9dfced0400cee915da5ec7592522e016a54b672ef4f68bbff9eac668bd880f36` |
| RV64 | `892c32b76e79572bafc7ae45aceed9b49295ffa7f3adaaab3136b2d1e3f3f2c4` |

Common procedures.dis64 SHA256 remains
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
Normal console shutdown uses SIGKILL after verified retirement markers; status
alone is not the packaged replay predicate. GitHub retains archives for14 days.
They contain Linux consoles, existing scalar images and gate logs, not new APKs.

[Inferno outgoing journal](dis64-inferno-journal.md) and
[FlashVM incoming journal](dis64-flashvm-journal.md) retain direct coordination.
