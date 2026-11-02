# Series0080: actual typed result stores on every returning path

## Implemented boundary

The new private constructor binds actual existing MOV stores through `0(32(fp))`
to canonical result signatures and numeric incoming states, then checks every
entry-to-RET path. Numeric local writes never substitute for result stores.
Source initialization, exact kind and Bool invariants remain mandatory. Loops
are analyzed without claiming termination; nonvoid functions conservatively
require at least one reachable RET. Final summaries publish atomically.

[Contract, preconditions, budgets and reproduction](dis64-bound-return.md).

No executable CALL/FRAME/RET/LEA activation, general pointer/indirect memory,
format IDs or production reader/ordinary Dis semantics change. In particular,
valid incoming result-address provenance is still a checked-caller obligation;
this result is a conditional callee-body proof, not an execution capability.
Trusted compiler local provenance and actual container binding remain explicit.
The remaining composition is prepared-frame ownership, argument/result binding,
retirement/unwind and shared whole-file C/RX positive/refusal goldens.

## Checks

1548 raw metadata/body cases (937 return cases plus611 prior catalog cases under
the stronger contract):388 accepted/1160 refused. All scalar kinds, same-width
wrong kinds, source initialization, pointer base/displacement, all256 mode bytes,
branches/loops, unreachable malformed bytes versus valid unreachable reads,
128-slot state, late refusal guards,64/65 functions and65536/65537 instructions
are covered. Independent numeric TOP/intersection and boolean TOP/intersection
proofs check the C numeric FIFO plus cut-graph traversal.26 active removal
controls must fail semantically under optimized nonrecovering UBSan.

Native ARM64/x64 and RV64/QEMU validation is pending at the source checkpoint.
Earlier611 bound numeric/14 controls,2029 numeric/13 controls and older target
gates remain in the native driver. No target success is inferred from a local
run or previous series.

FLASHVM-0069 independently replays0079:611 cases/all14 controls. Its replay and
hosted native metadata/directory-wait work remain separate peer evidence; no
scheduler/file adapter is imported here. Its proposal explicitly prefers a
reachable normal-return witness for the initial nonvoid subset; we agree and
implement that conservative private policy here without allocating wire IDs.
Actual caller ownership, result invalidation/unwind and bottom-up call summaries
remain before shared whole-file C/RX activation.

## JIT coverage (unchanged)

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

New-format file admission remains43/173 (24.86%), separate from opcode-entry JIT
coverage and from complete application/runtime readiness. Complex handlers stay
in C. This checkpoint does not deliver an Android APK or full managed GUI shell.
