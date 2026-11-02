# Private numeric definite initialization: checkpoint0078

Status: real scalar bytes plus trusted immutable declarations/incoming-signature
context. This composes numeric facts and entry-reachable CFG analysis; it is not
mixed callable-catalog admission, return guarantees or new file CALL execution.

## Entry and transfer

`include/dis64-local-initialized.h` reparses every actual instruction and derives
its typed operand facts before traversal. Last fallthrough/conditional nodes and
malformed unreachable instructions refuse. Input scratch is reset and never
accepted as supplied proof. Only the first node receives an entry state.

The existing version1 scalar signature is revalidated. Its at most64 incoming
arguments must match the declaration table's exact canonical prefix from byte64,
including alignment and kind; a same-width kind or different span cannot seed
an argument. Only these argument IDs initialize at entry. All other locals are
unset regardless of declarations. Actual argument establishment and binding of
signature/declarations to a canonical callable catalog remain trusted context
obligations. Signature result kind is validated but proves no return value here.

Every source AND LHS read must already be initialized. Reads are checked BEFORE
that instruction's write, including aliased source/destination and implicit old
destination under AXNON. The write then adds its complete exact slot ID. Branches
propagate to both successors; no path pruning from numeric values is claimed.
Unreachable numeric reads need no entry history, but all their byte/role/type
encodings still validate. Result-tail storage is not a declared local and is
not seeded; no callee result is silently assumed initialized.

## Intersection and convergence

The C implementation stores the first entry-derived incoming set and intersects
later predecessors. Changed sets requeue their node, even after it was visited;
already queued nodes are not duplicated. Sets can only lose bits after their
first installation. This handles delayed-path invalidation of a previously
optimistic read. A missing read can refuse immediately: later intersections can
never restore its bit. Unequal but unused local initialization is legal; this is
different from strict prepared-owner/type joins in the separate call-state model.

A ring of at most `instructions` entries is caller-owned. Each node visits at
most `slots+1` times, so no recursion, unbounded queue or unbounded fixpoint is
needed. Local loops may remain nonterminating at execution; bounded verification
is not a termination proof. This pass does not establish global call acyclicity.

## Resources and publication

Caps:65536 instructions,128 declared scalar slots and64 signature arguments.
Maximum scratch/queue on tested unsigned32 targets is4MiB. The checker allocates
no heap. All spans are accessible and disjoint; inputs stay immutable. Scratch
is disposable on refusal; output summary/guards remain byte-for-byte unchanged.

Budget units are deterministic information changes: one per reached node, one
per distinct outgoing edge role, and one per absent incoming slot fact. Total
at convergence is `reached + edges + sum(slots - initialized_at_node)`, at most
131*65536=8585216. Each newly removed bit is charged once; each actual revisit
requires a charged loss, bounding visits too. Static byte/type preflight is
separately bounded by the instruction/slot limits. A separate hard visit bound
is `instructions*(slots+1)`. This budget is not a guest instruction quota or CPU
cycle counter. Exact budget accepts; one below refuses without partial summary.

## Independent evidence

The Python oracle uses global synchronous intersections from TOP and immutable
sets, not C's first-edge FIFO. Only after greatest-fixed-point convergence does
it check reads. All2029 raw CFG/signature cases compare acceptance, exact summary
and EVERY reached node's four-word incoming set:1328 accepted,701 refused.

Cases cover delayed invalidation/requeue, unequal unused local sets, self reads,
old destinations, all four bitmap words through128 slots, canonical mixed scalar
arguments, same-width wrong kinds, wrong argument spans, closed/assigned loops,
unreachable reads versus malformed unreachable bytes, signature/code truncation,
trailing bytes, exact/exceeded budgets/capacity and65536/65537-node boundaries.

Thirteen semantic removal controls weaken intersection, each source/LHS read,
aliased read-before-write, local seeding, parameter kind/span, requeue, capacity,
budget, scratch reset, unreachable validation and transactional summary. Each
must reach predicate status10; compiler/signal/sanitizer failures do not count.
Capacity controls retain physical backing. Local optimized nonrecovering UBSan
passes the normal oracle and all controls. Native64-port runs the same gate and
controls for native ARM64/x64 and RV64/QEMU.

## Remaining boundary

Canonical compiler/catalog binding of declarations/signature/function intervals
is still required. The decoder currently has one common scalar frame context,
not FRAME/CALL/RET/LEA roles or callable result writes. Compose numeric facts
with strict prepared ownership and actual callee return guarantees before shared
whole-file C/RX goldens. Do not infer a serialized local-table section or IDs.
Current43-op np0 admission, ordinary Dis LEA and the accepted future module-entry
IEXIT/callable IRET design remain unchanged. No runtime/compiler includes this
private solver and no Android APK/GUI readiness is implied.

CI run41469285317 passes native ARM64/x64 and RV64/QEMU at38c1729, including all
2029 cases/13 controls and exact final incoming sets. Downloaded ZIP digests,
ELF64 machine/class and complete gate logs were checked. See
[target acceptance](native64-series0078.md#cross-target-acceptance-and-downloads).

## Canonical binding follow-up0079

[Catalog-bound numeric analysis](dis64-bound-numeric.md) now derives function
identity, interval, frame and incoming signature from raw canonical metadata.
The shared fixed-point loop is private; this standalone constructor retains its
raw instruction/seed checks. Local declaration provenance and real callee return
guarantees remain separate obligations.
