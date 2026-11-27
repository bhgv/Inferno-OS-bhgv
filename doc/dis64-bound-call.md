# Private linear caller / leaf composition: checkpoint0081

`include/dis64-bound-call.h` revalidates a complete raw code payload, canonical
exports/pool/types and trusted compiler declarations. It composes real root
FRAME/argument-store/LEA/CALL/result-copy bytes with freshly proved leaf bodies
from THE SAME immutable inputs. No client-supplied callee summaries or semantic
state events are accepted as proof. Final records/count publish atomically.

## Deliberately bounded call graph

Only the canonical module-entry function may call, and its body must be linear,
ending in one idle IEXIT. Branches/jumps, early exits, nested preparation and
ordinary computation inside a setup island refuse. Helpers retain the0080
numeric/branch/loop/typed-return checks and may not call. Root cannot call itself.
Thus the graph is a bounded root-to-leaf DAG, not the full proposed acyclic call
profile. CALL continuations are conditional on normal return, not termination.
All helper bodies are checked even if no root site calls them.

## Canonical setup and ownership

FRAME must use a literal exact Type ordinal and one of the complete native
pointer cells40/48/56. A disjoint forward scan finds that island's closing CALL;
the exact canonical callee entry selects the signature, NOT frame shape/ordinal
alone. This handles helpers sharing one frame Type but having different argument
signatures. CALL requires that same pending pointer and exact declared entry.
No branch can enter setup because root control transfers are closed entirely.

Argument MOVW/MOVL/MOVF stores through the pending pointer must address an EXACT
canonical argument start, derived from the callee signature/alignment from64.
Headers, padding, interiors and outgoing result tail refuse. Source must be a
matching initialized numeric local or allowed compact immediate; real immediates
and Bool literals outside0/1 refuse. Repeated typed stores to one argument are
legal; every required argument must be established before CALL. Both argument
bitmap words through64 parameters are tested.

The adapter derives events for the existing strict prepared-state model itself.
For a typed unknown Bool source, it checks BOTH0/1 transitions and exact-merges
the results instead of inventing a concrete value. Its source domain and
initialization are discharged by typed numeric producers/reads and canonical
leaf returns. Root has no incoming parameters, avoiding a new caller-value premise.
Compiler local-kind provenance remains TRUSTED, not attacker-supplied metadata.

LEA may bind only the caller's dedicated final aligned eight-byte cell to the
prepared callee's byte32 address slot, once, for the exact nonvoid result kind.
The model requires a complete eight-byte address write. This is a PRIVATE
execution contract: ordinary Dis LEA keeps its WORD behavior and the current
production Dis64 reader still refuses these calls. Explicit profile execution/
JIT-cache mode and C/RX address-width goldens remain before runtime activation.

New FRAME invalidates the old result. Only a successful normal CALL with the
proved leaf postcondition establishes its exact ready kind. A MOV from the
caller tail may then copy ONLY that kind into a matching declared local. It
initializes that local numerically; it cannot fabricate a result before CALL,
after a void call, during preparation or from an old preparation. Copies made
before the next FRAME survive as ordinary initialized locals. Header/temp/tail
storage never becomes an ordinary numeric declaration.

The root additionally accepts the existing direct ICVTWF int32-to-real producer:
AXNON, initialized Int or compact Int immediate source, exact declared Real
destination. This permits positive Real argument/result chains without inventing
an initialized Real seed. It does not enable a new file opcode or change any
production conversion/JIT/C handler.

## Proof order, errors and resource limits

All bytes and static setup roles are decoded first. Every leaf is then proved
using the existing numeric intersection and typed return-coverage kernels;
only afterward is the caller's numeric read-before-write proof solved. Derived
argument/copy facts are owned internal data, never supplied by an untrusted
caller. No partial earlier leaf record publishes if later leaf/root proof fails.

This is a normal-continuation STATIC proof. It neither executes FRAME/LEA/CALL
nor proves runtime failure/unwind cleanup. Root cannot exit with a pending
preparation. Failed runtime calls must retire ownership WITHOUT publishing a
result; existing abstract fail/abandon and native cleanup tests remain separate
until actual shared whole-file C/RX integration. No process-exit/return opcode
mapping is inferred from an abstract event.

The bound remains64 functions,65536 total instructions,128 local slots/function,
64 parameters and1MiB per raw section. Numeric+return work plus one root unit/
instruction is bounded by135*65536=8847360. Forward scans cover disjoint islands;
they are bounded static preflight, not an additional guest instruction quota.
No heap/recursion is used. Caller provides disjoint accessible decoded/numeric/
queue/output storage; all decoded scratch is overwritten from bytes. On tested
LP64 targets workspace is40+60+4=104 bytes/node,6.5MiB at65536 nodes, excluding
bounded local catalog/descriptor stack arrays and final output. Scratch is
always disposable on refusal.

Container provenance and immutable compiler declaration provenance remain caller
obligations. No proof cache is introduced; any future cache must bind ALL code,
metadata and declaration identities. Export names or equal frame layouts alone
cannot authorize a cached result or external/native invocation.

## Independent checks

Run `python3 utils/tests/dis64-bound-call.py`. Its reference independently parses
raw framing and partitions complete root islands before checking signature/
argument/binding/ready rules. A REFERENCE-ONLY root-exit projection preserves
all global PCs and original helper bytes for the prior independent canonical
leaf oracle. Every original root instruction is checked separately; no such
projection is used by C. Linear initialized sets are computed independently
from typed facts, and all22 output words/function compare exactly.

3338 vectors include1790 caller cases plus1548 prior return vectors under the
stronger linear-root contract:387 accepted/2951 refused. Tests include all four
scalar kinds, two different signatures sharing a frame Type, chained calls,
result invalidation versus copied-local survival,64 mixed parameters, both
argument words, bit128, wrong addresses/types/pointers, missing/duplicate setup,
all256 mode bytes, raw section truncations/trailers, exact/below quotas,
64/65 functions and65536/65537 total instructions.29 active controls must reach
semantic status10, not compiler/signal/sanitizer failure.

[Target report and archives](native64-series0081.md). General caller CFGs,
non-leaf/nested calls, bottom-up arbitrary DAG composition and actual runtime
retirement/address-width/file-call goldens remain before executable admission.
