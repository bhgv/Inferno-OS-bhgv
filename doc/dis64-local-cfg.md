# Private entry-seeded semantic CFG: checkpoint0076

Status: executable composition of trusted semantic events and owned local edges,
not a decoded Dis64 instruction verifier, loader admission or real file CALL.
This builds on the catalog/flow/state preflights and FLASHVM-0064/0065's agreed
composition boundary. No format, opcode or feature IDs are allocated.

## One immutable function context

`include/dis64-local-cfg.h` checks one bounded function span under one immutable
plan. The caller supplies its canonical function identity and absolute base PC;
the future catalog adapter must derive these, not trust serialized annotations.
Edges use local indices and cannot leave this span. Preparation identity is the
absolute preparation PC+1. A setup-interior node names its earlier Prepare; static
checks require the exact callee/frame/signature facts from that preparation.

Every abstract node is checked BEFORE traversal, including unreachable nodes.
Unknown/noncanonical events, invalid kinds/bool facts, bad Bind offsets/width,
bad setup ownership, malformed terminal edges, final conditional/fallthrough,
and jump/branch entry into any setup interior refuse. Branches within setup are
closed too. This does not inspect actual encoded instruction modes or operands.
Zero-action nodes represent trusted pure operations with no modeled state effect;
a future decoder must justify that classification and prove numeric local facts.

Traversal always seeds the first node with zero entry state. Caller-supplied
scratch is poisoned in tests and its seen markers are reset on every analysis.
No supplied incoming state or earlier function's scratch can establish history.
At every reached node, pending preparation must match that node's setup owner.
The previous semantic transition checker then derives its output state.

The first edge reaching a node stores its derived state and queues it once.
Later edges must join IDENTICAL live states. Unequal facts refuse; equal back
edges do not queue another visit. Transfer is deterministic under immutable
facts, so this conservative policy needs no widening or iterative fixpoint.
Local closed loops are legal; a changing-state back edge refuses. This is bounded
verification, not program termination or progress proof. Recursive call graphs
remain a separate previously checked global boundary.

Return/failure nodes have no continuation. Every reached nonvoid Return needs
the function's own incoming assignment; a completed nested call only establishes
its outgoing result kind. Failure/abandon cannot revive the old result. Bodies
with no returning path are not required to terminate.

## Work and publication

Bounds:1..65536 nodes, base+count at most65536, function identity0..63, scratch
and queue capacity at least count. Each reached node is queued once; each has at
most two edges. An explicit1..196608 propagation budget charges one unit per
processed node and edge comparison, including repeated/back edges. Static
preflight separately checks all count nodes under the existing64-function/
64-argument plan bounds. These are resource-policy limits, not new wire limits.

The caller owns disposable scratch (`count` state/seen records plus `count`
queue words;2.5MiB at the maximum on tested unsigned32 targets). The checker
allocates no heap and uses no recursion. All spans are accessible, immutable
where input, and disjoint. Scratch may change on refusal; the final summary and
its guards remain byte-for-byte unchanged until complete success. A limit
refusal cannot publish a partly trusted program.

## Independent evidence

The Python reference uses DFS with immutable incoming-state dictionaries; C uses
a FIFO and mutable caller scratch. The reference composes the independent set-
based state oracle from `dis64-local-state.py`. Its4005 graphs yield1355 accepted
and2650 refused, with exact successful summaries and transactional refusal.
Coverage includes setup/site mistakes, both argument-mask halves, typed returns,
stale reads after abandon, failure after binding, nested calls without incoming
assignment, differing joins, closed/changing loops, malformed unreachable nodes,
exact/exceeded work/capacity/base/function limits and65536/65537-node boundaries.

Eight semantic removal controls weaken entry seeding, unreachable preflight,
setup-target rejection, reached setup ownership, exact joins, scratch capacity,
work budget and scratch reset. Each must reach active predicate status10;
compiler, signal or sanitizer failure is not a successful control. Mutated
capacity probes retain physical backing so the control fails semantically rather
than intentionally accessing invalid memory. Local optimized nonrecovering
UBSan passes the full oracle and all controls.

`native64-port.py` runs this gate and its eight controls on native ARM64/x64 and
RV64/QEMU. Copying this fixture elsewhere also requires the existing Python
state oracle; no runtime/compiler source includes the new header.

## Still before file admission

Derive immutable function/callee facts and local spans from checked catalogs and
ALL decoded instruction operands; prove source kinds, scalar widths and numeric
local definite assignment; classify pure operations; compose every callee's
return guarantee and the global call graph. Bind the abstract function context
to the exact catalog interval rather than an arbitrary supplied identity. Then
shared whole-file C/RX positive/refusal goldens precede activation. The existing
43-op np0 file profile and ordinary Dis LEA remain unchanged.
