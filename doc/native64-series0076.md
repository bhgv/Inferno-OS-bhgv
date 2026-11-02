# Series0076: entry-seeded semantic CFG composition

Source: `9677ed5b70de82bc2356db0a233f334ca61a65d1` on `experiment`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41451121322

## Implemented

The new private CFG checker composes the previous prepared-call state model
with one immutable function's local edges. Every abstract node is statically
validated first, including unreachable ones. Fallthrough/conditional boundaries,
exact preparation references, setup-interior branch exclusion, kinds/bool facts,
binding geometry and terminal structure are checked before propagation.

The first node receives only zero entry state. Disposable scratch is reset,
not trusted as incoming proof. Each node queues once; later incoming edges must
join the exact same live state. Equal back edges need no repeated iteration;
changed facts refuse under the conservative initial policy. Every reached node's
pending preparation must match its setup owner. Returning nonvoid paths require
their own incoming assignment, not just a completed nested call's result.

Explicit caps cover65536 nodes, absolute base/span, function identity0..63,
complete scratch/queue capacity and1..196608 node/edge work units. All input
spans are disjoint and immutable. Scratch is disposable on refusal; output
summary/guards remain unchanged until full success. No heap or recursion is
used by the checker. Local closed loops are not termination proof or permission
for recursive global call graphs.

## Independent checks

The Python oracle uses DFS and immutable state dictionaries; C uses FIFO and
mutable scratch. Semantic transfers compose the independent Python SET-based
state oracle. The4005 graphs yield1355 accepted and2650 refused, including exact
summary comparisons and poisoned scratch/output guards. Cases cover both
argument-mask halves, nested calls without incoming assignment, stale result
read after abandonment, failure after binding, setup targets, conflicting joins,
closed/changing loops, malformed unreachable nodes and exact/exceeded resources,
including65536/65537-node boundaries.

Eight active removal controls weaken entry seeding, unreachable preflight,
setup-target exclusion, reached setup ownership, exact joins, capacity, work
budget and scratch reset. Each must reach semantic predicate status10; compiler,
signal and sanitizer failures do not count. Local optimized nonrecovering UBSan
passes normal oracle and all controls. The complete native64-port matrix runs
these gates after classic mk builds on native ARM64/x64 and RV64/QEMU.

## Shared format and remaining work

FLASHVM-0065 confirms the earlier composition boundary and independently selects
and replays the catalog/flow/state layers. Its owner-lifetime/managed-turn work
is peer-reported, not independently reproduced here. Our INFERNO-0076 describes
this new CFG layer and its test-import dependency directly in the shared journal.

This checker consumes TRUSTED semantic events, not decoded wire instructions.
The caller still supplies immutable plan/function/base facts; catalog adapters
must derive them. Numeric source/span/width/definite-assignment proofs, pure-op
classification, actual callee return guarantees and global call-graph composition
remain before whole-file C/RX positive/refusal goldens and activation. No runtime
or compiler source includes this private header.

| Verification boundary | Current evidence |
|---|---|
| Catalog geometry |628 private cases, two controls|
| Abstract function/call graph |1540 vectors, five controls|
| Semantic preparation/result state |98032 records,13 controls|
| Entry-seeded semantic CFG |4005 graphs, eight controls|
| Decoded operand/local source/span adapter |Still required|
| Whole-file CALL C/RX goldens and activation |Still closed|

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

Opcode counts are unchanged. Shared file admission remains43/173 (24.86%);
file CALL/FRAME/RET/LEA, ordinary Dis LEA and all registry IDs stay unchanged.
These are guarded opcode-entry counts, not a percentage of complete runtime
or format readiness. Complex handlers stay in C. This checkpoint is a Linux
scalar console validation, not Android APK/GUI or general managed application
startup evidence.

## Cross-target acceptance

All three CI jobs pass for the source SHA above: native Linux ARM64/x64 and
Linux RV64/QEMU. Every target passes4005 CFG graphs (1355 accepted/2650 refused)
and eight semantic controls, plus98032 state/13 controls,1540 flow/five controls
and628 catalog/two controls. x64 C fixtures use optimized nonrecovering UBSan.

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41451121322/artifacts/13144154745) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41451121322/artifacts/13145532179) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41451121322/artifacts/13144657704) |

All archives were downloaded. ZIP SHA256 digests match GitHub metadata; consoles
are ELF64 machines183/62/243. Logs retain241 helper sources/482 whole C/JIT
processes per target,21504 fresh-message cases,5376 caught-handler cases and896
terminal-call cases. Packaged x64 loop/case/procedures additionally replay locally
with JIT off/on: six expected-module, empty-exception and empty-scheduler checks.

The same procedures.dis64 SHA256 appears on all three targets:
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
Archives include the Linux console tarball, existing restricted-profile examples
and complete logs, with14-day GitHub retention. They do not establish Android
APK/GUI or new file-call admission.

A supplemental LOCAL optimized UBSan probe reuses the same driver/oracle with
four one-node graphs: final fallthrough, final conditional and out-of-range jump
refuse; a closed self-jump accepts. This is additional local boundary evidence,
not part of the4005-case cross-target count.
