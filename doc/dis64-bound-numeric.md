# Catalog-bound numeric initialization: checkpoint0079

This private checker binds actual scalar instruction bytes and immutable compiler
local declarations to canonical raw exports, string pool, frame types and incoming
signatures. It does not admit executable callable Modules.

## Binding and interval checks

`include/dis64-bound-numeric.h` revalidates raw metadata through the existing
catalog/structural codecs. Function declarations must match exactly one canonical
(symbol ID, entry PC, frame Type ordinal), even when two Types have equal shapes.
Exports and declarations may arrive in different orders. Sorted canonical entry
PCs partition the complete code; all instruction bytes, including unreachable
ones, receive their actual function's frame, slot spans and role.

Each signature supplies only that function's canonical argument seeds. Every
branch target must remain inside its owner's global interval BEFORE rebasing to
local PC. Fallthrough/conditional edges cannot cross the next entry. The private
numeric pass permits IEXIT only in the module-entry interval and exact empty IRET
only in callable intervals, following FLASHVM-0066/0068. This does not expand the
production 43-op loader gate or change ordinary Dis behavior.

After full byte/type preflight, the prior intersection solver runs separately on
each function's scratch slice. Root writes never seed a helper. Scratch/queue
state is reset, not accepted as supplied proof. A single module budget charges
all functions' reached nodes, edge roles and missing incoming numeric facts.
Budget/resource bounds remain local verifier policy, not serialized format IDs.

Final records and count publish only after EVERY function succeeds; scratch may
change on refusal. There is no allocation or recursion in the C checker. Bounds
remain64 functions,65536 total instructions,128 numeric slots per function and
1MiB per raw section. Caller supplies accessible immutable, disjoint input spans
and scratch/output backing; actual container-section provenance is its obligation.

## Conditional scope

Kinds and declaration provenance remain TRUSTED immutable compiler facts, bound
by canonical identity and checked frame geometry. These declarations are not an
untrusted client table or a new serialized local-table format. Result-tail is not
an initialized local. A nonvoid signature plus numeric IRET acceptance does NOT
prove that a return value was stored. Prepared ownership joins remain strict and
separate from numeric intersection. Actual return-store guarantees and mixed
FRAME/CALL/LEA composition precede whole-file C/RX call goldens and loader changes.

The standalone initialized checker keeps its prior public raw-code validation;
its fixed-point loop is now a shared private kernel invoked only after each
owned constructor has decoded and validated facts/edges/seeds. It is not an
admission API for arbitrary client facts.

## Reproduction

Run `python3 utils/tests/dis64-bound-numeric.py` from the repository. Default C
fixtures use optimized nonrecovering UBSan. The Python reference independently
parses raw metadata and actual code, checks global ownership and re-encodes each
local interval for the prior synchronous TOP/intersection oracle. Reference-only
IRET-to-terminal translation occurs AFTER canonical callable-role checks; no
production loader translation is introduced. Lossless Rune32 pool values retain
every bit, including non-Unicode values, as required by the agreed codec.

Fourteen active `--control` choices remove symbol, entry, exact frame ordinal,
signature, framebytes, interval, rebasing, root-return, callee-exit, fallthrough,
output/work capacity, aggregate budget or atomic publication checks. Each must
fail the semantic predicate with status10; compiler/signal/sanitizer failures do
not qualify. Native target driver runs the normal suite and every control.

See [series0079](native64-series0079.md) for target evidence and remaining scope.
