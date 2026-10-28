# Bounded native-stack use in descriptor and initialization queries

Run `python3 limbo/tests/type-walks.py` from the repository root.
`TYPE_WALK_CASE` selects shallow, map, zero or reject; `TYPE_WALK_WIDTH` selects
4 or 8, otherwise both GC slot widths run. `TYPE_WALK_TYPES` selects an alternate
production types.c for before/after probes.

## Production behavior

`descmap` and `tdescmap` previously called each other for every nested value
aggregate. `tmustzero` and `mustzero` likewise recursed through every nested tuple
or ADT while deciding whether a local declaration needs reference initialization.
Even a tiny one-slot layout can contain a deep chain of nested value aggregates,
so descriptor size bounds did not bound the native call stack.

Descriptor construction now uses checked, geometrically grown continuations.
Each frame retains its absolute offset, highest marked pointer offset and the
pending declaration or aggregate. Aggregate recursion marks remain active until
all its fields return, so illegal by-value cycles still fail while a shared Type
can be revisited at another offset. Normal completion restores all marks. The
existing declaration filters, offset/capacity checks, alt/case table mapping,
debug event order and maximum-offset result are retained. Invalid Type kinds
are rejected before indexing the classification table.

The initialization query has a separate compact continuation stack. It retains
its original field order, pointer classification and early true result, includes
all declarations as before, and does not change Type recursion marks. Both full
false traversals and early true exits free their private work storage. It assumes
the compiler has already rejected illegal cyclic by-value types; unlike the
mapper, this query does not add cycle detection or memoize shared subgraphs.

The two traversals intentionally retain their different semantics: descriptor
mapping skips constants, functions, module Type declarations and linked locals;
the initialization query checks all declaration Types. Exception and switch-table
handling also remains specific to descriptor generation.

Allocation growth checks its byte count before multiplication and allocation.
The ordinary compiler allocator remains responsible for fatal allocation failure.
Fatal compiler exits do not introduce a recoverable rollback protocol for active
Type marks. Valid input graphs are borrowed and otherwise left unchanged.

## Verification

The driver extracts the production traversal bodies with real compiler headers.
Shallow construction independently records the expected pointer leaves while
creating disjoint nested regions, including skipped declarations, picked Types,
scalar values and references. A shared aggregate case separately checks that
mark clearing permits repeated visits at distinct offsets.

| Family | Scenarios per slot width/profile |
| --- | ---: |
| Independently constructed layouts, queries and shared aggregate | 2,001 |
| Deep mapping through 100,000 nested values | 5 |
| Deep initialization queries through 100,000 nested values | 5 |
| Cycles, invalid spans/kinds, arithmetic rejection and nil queries | 9 |
| Total | 2,020 |

Each shallow scenario checks exact bitmap bytes, the maximum marked offset,
canaries, zero-initialization answers and unchanged Type/Decl snapshots. Deep
queries include deferred empty-value siblings to force heap stack growth, both
pointer-free completion and early pointer success. All deep cases run with a
512 KiB native stack. Private allocation accounting requires every successful or
early-return traversal to release its work storage. An unread later declaration
also checks pointer-result short-circuiting.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass under NDEBUG with sibling-call
optimization disabled: 12,120 scenarios across both slot widths. The optimized
profile also treats pointer/int conversion diagnostics as errors. LSan process
inspection is disabled; address instrumentation and explicit owner checks remain
active. These are LP64 host tests with logical 4-/8-byte GC map slots, not a
32-bit executable or a booted target VM.

Four parent-source executions against baca741 reproduce ASan stack-overflow:
map and zero traversals with each slot width. Parent shallow maps/queries also
pass the independent 2,000-layout oracle at both widths. Expected fatal rejection
probes must exit through the fixture's compiler fatal handler; a signal or
sanitizer failure does not count as successful validation.

The existing descriptor suite passes 2,101 cases at each slot width, including
switch tables, pointer boundaries, descriptor interning and exception cleanup.
AST diagnostic regressions and native bootstrap also pass. Bootstrap builds
ELF64 tools, regenerates the real parser/decoder and compiles all 17 Limbo units.
The new driver is registered in the core compiler/interpreter suite.

No native opcode encoder changes are included. AMD64 and ARM64 readiness remains
173/173 complete JIT+C entries and 166/173 guarded native fast paths. Full VM
compiler/loader/frame/GC ABI acceptance is still separate from these host tests.
Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources remain unchanged.
