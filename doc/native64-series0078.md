# Series0078: numeric definite initialization over real scalar CFGs

Source: `38c1729feee31411555336630c6341bf561c82b8` on `experiment`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41469285317

## Implemented boundary

The new private solver decodes actual scalar code and derives all typed operand
facts before traversal. Entry initialization comes ONLY from revalidated canonical
incoming signature arguments, matched against exact declaration-prefix spans and
kinds. Declarations alone do not initialize locals. Context/signature/catalog
provenance remains an explicit trusted premise, not new serialized metadata.

All source and LHS reads precede writes, including aliased sources and AXNON old
destinations. Incoming numeric sets INTERSECT at joins. A node is requeued after
loss even if previously visited; queued nodes are deduplicated in a bounded ring.
Unused unequal initialization states are legal. This is distinct from strict
prepared-frame/type joins. Result-tail/callee initialization is not invented.

Every byte/role/type encoding is checked, including unreachable instructions.
Only reachable reads need initialization. Closed loops are allowed; termination,
actual callable roles and return guarantees remain separate properties. No loader
or compiler source includes this private pass; shared file calls are not enabled.

## Bounded deterministic work

At most65536 instructions,128 slots and64 canonical arguments are analyzed.
Each node visits at most slots+1 times; ring occupancy is bounded by instructions.
No heap or recursion is used by the checker. Caller scratch/queue uses4MiB at the
maximum on tested unsigned32 targets. Inputs and output/scratch spans are disjoint.
Scratch is disposable on refusal; summary and guards do not change until success.

Budget charges reached nodes, unique edge roles and missing incoming slot facts.
Its final value is reached+edges+missing, independent of traversal ordering and
bounded by8585216. Each information loss is charged and bounds requeue work; a
separate hard visit limit remains. This is not the guest instruction quota.

## Independent checks

The Python reference uses synchronous global greatest-fixed-point intersection
from TOP; C uses first-edge FIFO propagation. All2029 raw CFG/signature cases
compare acceptance, exact summary and EVERY reached incoming four-word set:
1328 accepted and701 refused. Cases include delayed-path requeue, unequal unused
locals, old/self reads, all four words through128 IDs, canonical mixed arguments,
same-width wrong kinds/spans, loops, unreachable reads versus malformed bytes,
truncation, exact/exceeded budget/capacity and65536/65537-node boundaries.

Thirteen active controls weaken intersection, source/LHS checks, self-read before
write, fabricated local seeds, parameter kind/span, requeue, capacity, budget,
scratch reset, unreachable preflight and transactional summary. Each requires
semantic predicate status10, not compiler/signal/sanitizer failure. Local optimized
nonrecovering UBSan passes normal oracle and all controls.

FLASHVM-0067 reiterates the canonical span/kind, intersection and read-before-write
boundary. Its earlier17544 operand replay is retained from0066, not double counted.
Its hosted bounded worker pool is peer-reported native Linux x64 runtime work;
no scheduler implementation is imported or ARM/RV pool evidence inferred here.

## Remaining composition and coverage

Canonical function/catalog/interval binding of context and actual callee result
writes/return guarantees remain before mixed FRAME/CALL/RET/LEA handling and shared
whole-file C/RX positive/refusal goldens. Signature result kind alone proves no
return value. No new local-table/profile/opcode IDs or current flat-profile tail
requirement is introduced. Ordinary Dis and the agreed future module-entry IEXIT/
callable IRET policy remain unchanged.

| Boundary | Private evidence |
|---|---|
| Catalog geometry |628 cases/two controls|
| Abstract function/call graph |1540 vectors/five controls|
| Prepared ownership/result state |98032 records/13 controls|
| Entry-seeded semantic CFG |4005 graphs/eight controls|
| Actual typed scalar operands |17544 payload/layout cases/12 controls|
| Numeric definite initialization |2029 CFG/signature cases/13 controls|
| Canonical callable composition and real file-call execution |Still required|

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

Opcode counts are unchanged. New-format file admission remains43/173 (24.86%).
These opcode-entry percentages are not complete runtime/format readiness.
Complex handlers stay in C. This checkpoint is Linux scalar console validation,
not Android APK/GUI or general managed application acceptance.

## Cross-target acceptance and downloads

All three jobs pass at source38c1729: native Linux ARM64/x64 and RV64/QEMU in
run41469285317. Every target passes2029 numeric CFG/signature cases (1328 accepted,
701 refused), all13 controls and exact reached-node incoming sets. Earlier17544
operand/12 controls,4005 CFG/eight controls,98032 state/13 controls,1540 flow/five
controls and628 catalog/two controls pass too. x64 C fixtures use optimized
nonrecovering UBSan. These are separate target executions, not peer pool results.

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41469285317/artifacts/13153894750) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41469285317/artifacts/13156155812) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41469285317/artifacts/13155540891) |

Every archive was downloaded; ZIP SHA256 matches GitHub metadata and extracted
consoles are ELF64 machines183/62/243. Logs retain241 helper sources/482 whole
C/JIT processes per target,21504 fresh-message cases,5376 caught-handler cases
and896 terminal-call cases. Packaged x64 loop/case/procedures also replay locally
with JIT off/on: all six report expected module, empty exception and empty
scheduler after retirement.

The common procedures.dis64 SHA256 remains
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
Artifacts contain the Linux console, existing restricted-profile examples and
complete logs, retained by GitHub for14 days. No new CALL or Android APK/GUI
admission follows from these results.
