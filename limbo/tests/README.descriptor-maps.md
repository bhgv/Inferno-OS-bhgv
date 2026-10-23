# Compiler GC-map extent and bounds

Run `python3 limbo/tests/descriptor-maps.py` from the repository root.
MAP_CASE selects permutations, random, exception, tables or bounds.

The production descmap/tdescmap, descriptor interning, gendesc and actual excdesc
exception-cleanup builder run against real compiler type/declaration layouts.
Each 4/8-byte logical word model checks 2,088 map cases before the additional
constructor cases documented in README.descriptor-allocation.md.

Expected bitmap bytes are computed independently from pointer-slot indices.
Tests include all 24 orders of four spaced roots, nested aggregates, 2,000
arbitrarily ordered/reused-slot lists, case-string and alt tables, exact map
interning and native allocation ownership. The excdesc test uses two locals
at slots 0 and 18. The production builder reverses their declaration list;
the former descmap returned the last visited offset and emitted only one map
byte, losing the root bit in byte three. The fixed code returns the highest
marked offset regardless of visitation order.

The internal descmap/tdescmap APIs now carry the complete descriptor byte size
through recursion. Each bit is checked for alignment and a complete in-bounds
pointer slot before writing. Checked offset addition prevents overflow during
nested descent. Five fatal cases reject out-of-range, negative, unaligned,
partial-pointer and excessive nested offsets. The original code independently
triggers ASan heap-buffer-overflow. Post-write length diagnostics are retained
as an additional consistency check, rather than the first bounds check.

MAP_TYPES_SOURCE selects a baseline. The driver adapts only the old internal
function signatures to carry an unused capacity argument, preserving old map
logic for regression comparisons. The original exception-cleanup pipeline
fails its independently expected three-byte map. Real pointer maps are interned
and consumed by production compiler code; declaration creation and selected
logging/type-usage boundaries have bounded adapters.

ASan and strict UBSan are enabled by default. Live map/descriptor allocations
are counted, with LeakSanitizer process inspection disabled. Terminating fatal
children may emit sandbox ASan no-return bookkeeping warnings. This is compiler
map generation coverage, not runtime GC scheduling or JIT machine execution.
The 8-byte logical model does not activate a new ABI.

Previously emitted Dis files with truncated maps must be recompiled. The missing
root information cannot be recovered from the truncated descriptor alone.
