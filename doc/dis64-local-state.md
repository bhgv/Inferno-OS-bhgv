# Experimental local-call state model: checkpoint0075

Status: private abstract semantic transitions and strict joins, not a file
instruction verifier or executable call admission. This follows the initial
subset decisions accepted in FLASHVM-0063. It allocates no IDs and preserves
ordinary Dis LEA and the shared43-op np0 file profile.

## Representation and ownership

`include/dis64-local-state.h` accepts bounded immutable abstract callee facts:
exact frame ordinal, scalar result kind and up to64 argument kinds. Caller
geometry identifies the dedicated final aligned eight-byte result cell after
its argument span. Facts are rechecked before transitions and joins. The model
does not derive them from exports, frame maps or decoded operands; composition
with the catalog and control-flow preflights remains separate.

The32-byte state uses ordinary unsigned32 fields, independent of VM WORD. A
nonzero preparation identity represents a checked source PC+1, bounded65536.
One callee ordinal, result binding and two32-bit argument masks belong to that
preparation. The current function's incoming result-assignment bit is separate
from its completed outgoing result kind. Zero initialization is live entry.
Only transitions from that entry can establish semantic provenance; accepting
the canonical shape of supplied raw state alone does not prove its history.

## Transitions

| Abstract event | Preconditions | Effect |
|---|---|---|
| Prepare | Live idle state, bounded site, exact callee/frame | Acquire one preparation; invalidate old outgoing result |
| Argument | Pending owner, valid position, exact kind, bool0/1 | Mark that argument initialized; repeated typed stores are legal |
| Bind | Pending nonvoid owner, no prior binding, exact result kind | Bind caller cell to callee byte32, width8 |
| Call | Exact pending callee/frame, every argument initialized, nonvoid binding present | Model successful return; retire preparation; publish exact outgoing result kind |
| Read | Live idle state with completed result of exact kind | Preserve that readable result until reuse or retirement |
| Store | Live idle state, exact nonvoid incoming result kind, bool0/1 | Mark this function's own return result established |
| Abandon | One pending preparation | Retire it and invalidate its result; preserve separate incoming assignment |
| Return | No preparation; nonvoid incoming result established | Retire all outgoing state and terminate the path |
| Fail | Any live state | Retire preparation and all result facts; terminate the path |

Returned/failed states cannot transition or join into a continuation. A second
Prepare, missing/duplicate Bind, premature Call, pending/stale Read, abandoned
result, wrong frame/kind or missing return assignment refuses transactionally.
Unused event fields must be zero. Unknown scalar kinds and bool values outside
0/1 refuse; equal widths do not make int and bool, or big and real, interchangeable.

Call models SUCCESSFUL semantic return, not machine execution or an in-flight
callee. The future verifier must establish the callee body's result guarantee;
a real error must follow the failure path rather than publish a partial result.
Store assumes its abstract source value is established: this header does not
prove numeric local initialization, result-store byte spans/widths or dynamic
scalar arithmetic. Only the address-binding width is checked directly here.
For bool, the event supplies a concrete value; no arbitrary wire proof flag or
unchecked dynamic bool operand is introduced.

## Joins and publication

Only identical live states join. Preparation site/callee, argument initialization,
result binding, completed result kind AND incoming-result assignment must agree.
This is deliberately conservative: it does not intersect unequal initialization
sets or infer facts from numeric-width equivalence. A future CFG walker must
seed entries and propagate these operations; this checkpoint does not walk a CFG
or prohibit branch entry into setup interiors by itself.

Output is published only after complete validation. Refusal preserves all bytes
of caller output. Step allows output equal to its input; join allows output equal
to either input. State objects must be disjoint or exactly aliased, never
partially overlapping. Plan/event storage remains disjoint and immutable. The API uses
fixed local scratch, no heap, recursion, serialized pointer or managed reference.

## Evidence and limits

The independent Python reference represents argument initialization as a SET and
preparations as immutable semantic owners, rather than the C bitmaps/mutable
candidate. It supplies98032 transition/join records:20250 accepted,77782 refused,
including57330 joins. Every record checks disjoint guarded output and in-place
output; joins also alias the right input. Cases cover all result kinds, argument
counts0/1/2/31/32/33/63/64, both bitmap halves, result reuse, terminal paths, each
changed state field, invalid descriptors/geometry and deterministic mixed traces.

Thirteen removal controls separately weaken argument completion, result binding,
reuse invalidation, argument kind, bool range, incoming-result assignment,
terminal retirement, join identity, unused-field canonicality, single preparation,
address destination/width, frame identity and result kind. Each must reach an
active predicate with status10, not compilation/signal/sanitizer failure.

The target gate is `utils/tests/dis64-local-state.py`; native64-port runs it and
all controls after classic mk build on native ARM64/x64 and RV64/QEMU. Local
optimized nonrecovering UBSan passes the full oracle and all controls.

Still required before admission: derive immutable facts from checked catalog and
decoded operand roles; enforce numeric local spans/definite assignment, setup
interior ownership and exact prepared-frame writes; propagate states over CFG;
compose callee return guarantees; then shared whole-file C/RX positive/refusal
goldens on all targets. No loader/compiler/runtime call path uses this header.

FLASHVM-0064 independently replays the98032 records/all13 controls at source
71a259a under optimized nonrecovering UBSan and accepts this trusted semantic
model/strict join as the conservative initial policy. Our CI run41440788038
separately passes native ARM64/x64 and RV64/QEMU. See
[target evidence and downloads](native64-series0075.md). Neither acceptance
proves decoded operands, entry-seeded state history or actual file CALL execution.
