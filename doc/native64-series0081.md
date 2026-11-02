# Series0081: raw linear caller setup bound to proved leaf returns

## Implemented

The new private whole-payload constructor derives canonical FRAME/argument MOV/
LEA/CALL/result-copy facts and strict ownership transitions from actual bytes.
Exact callee entry chooses signature; equal frame shape/type cannot substitute.
Every leaf body is freshly proved from the same metadata/code inputs before the
caller's numeric continuation proof. Final records/count publish atomically.

[Full contract, bounds and reproduction](dis64-bound-call.md).

The bounded graph is a linear module-entry caller plus noncalling helpers.
Helpers retain branches/loops and0080 typed return coverage. Multiple root calls,
all scalar argument/result kinds, prepared-pointer cells40/48/56, both argument
bitmap words and repeated typed argument stores are checked. New preparation
invalidates the old result; copied numeric locals remain initialized. Existing
ICVTWF is privately classified as an exact Int-to-Real producer for real-valued
positive call chains, without fabricating a seed or changing a runtime handler.

This does NOT execute new-format calls or prove dynamic failed-call cleanup.
Nested calls, general caller CFGs, arbitrary DAG summaries, actual FRAME/LEA8/
CALL retirement/unwind/cache-mode integration and shared whole-file C/RX goldens
remain. Compiler declaration and immutable container provenance remain trusted
obligations. No production loader/reader, ordinary Dis semantics, wire identifiers,
JIT emitters or complex C handlers change. No proof cache is introduced.

## Checks

3338 raw metadata/code vectors (1790 caller cases plus1548 return vectors under
the stronger contract):387 accepted/2951 refused.29 active semantic controls
cover canonical identity, capacities/budget/atomicity, temp/frame/call pointers,
result binding source/destination/width, argument offsets/completeness/reads,
pending isolation, linearity, stale ready state, result kind/local write, actual
leaf guarantees/address and exact conversion source/destination kinds.

Local optimized nonrecovering UBSan passes. Workspace is104 bytes/node on LP64:
40 decoded+60 numeric+4 queue,6.5MiB at65536 nodes, plus bounded stack/output.
Native ARM64/x64 and RV64/QEMU validation is pending at this source checkpoint.
The prior1548/26,611/14,2029/13 and older gates remain in the native driver.
No target execution is inferred from local results or previous series.

FLASHVM-0069 remains the latest incoming review at this source checkpoint.
Its0079 owning-source replay and hosted metadata/file-wait work remain separate
peer evidence; no scheduler/descriptor adapter is imported into Inferno.

## JIT coverage (unchanged)

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

New-format file admission remains43/173 (24.86%), distinct from opcode-entry JIT
coverage and complete application readiness. This is a private verifier/Linux
scalar-console checkpoint, not an Android APK or full managed shell/GUI release.
