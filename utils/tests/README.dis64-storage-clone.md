# Rooted Dis64 storage clones before executable Module adoption

`clone64storage(source)` returns a separately releasable opaque storage lease.
The caller retains ordinary VM exclusion, keeps the original genuine lease alive
and unchanged throughout the call, and releases each successful result exactly
once with free64storage. Source/data/policy/native metadata are not asynchronous
borrowers or an executable Module capability. No file/cache/import/JIT admission
changes. Empty native storage is supported without allocating a managed object.

The clone retains each original descriptor identity at its original ordinal in
an independently owned Type-pointer array. The actual MP Heap descriptor selects
its type without assuming ordinal0. Before allocation, one table retain and the
MP descriptor's additional heapz retain are reserved. Official preparations and
clones have distinct descriptor identities within each table; opaque callers
cannot mutate this invariant. A fresh heapz MP starts nil/zero, is adopted and
rooted before production newmp recursively copies fields. Ordinary managed refs
retain/share according to existing C semantics; arraycpy creates native recursive
array copies. Protected failure retires the complete partial clone and its root,
preserving source data, Type counts and roots. Complex copying stays in C.

Run `python3 utils/tests/dis64-storage-clone.py`. The fixture reuses independent
raw structural bytes from0125 plus MP descriptor ordinal0/64 variants.33 graph
scenarios execute66 ordinary graph clone calls,131 individually injected allocation
refusals,2 genuine nested body refusals,513 actual collector epochs and31993
active predicates per WORD32 and WORD32-file model. A separate successful MP
retain-boundary case and four preflight refusals test both required Type retains;
nil/H sources refuse without allocation. A native empty metadata-only context
proves legacy-model refusal before allocation; no Heap/Type is fabricated for it.

Actual dtype/heapz/nheap/newmp/arraycpy/copyarray/heaparray/initarray/freearray,
complete collector and emu kprotect/krethrow execute. Native pool geometry, guarded
allocator, diagnostic and platform jump remain bounded services. Genuine source
and clone objects are created by actual heap functions. Input image bytes are
poisoned after preparation. Every descendant pool allocation can complete actual
GC epochs under rungc(nil). Clone/source roots and arraycpy's intermediate roots
preserve the graph. Independent data survives original-lease retirement; ordinary
leaves are shared with the exact retains, while two nested array levels have new
headers/data and preserve scalar bits and element Type ownership.

Every successful clone allocation position is forced to refuse, including owner,
Type array, MP, root and nested array/root allocations. The partial graph returns
to the original source-only pool/native/root baseline. Genuine ULONG_MAX leaf
retains refuse inside ordinary and nested array copy bodies and unwind real
protected frames. Source scalar/reference bytes and Type counts stay unchanged.
Root cache metadata is drained separately from active roots; native guard bytes
and Tarray/element Type baselines are checked. Ordinary hosted LSan remains enabled.

Six actual-source omissions (`retain`, `pin`, `copy`, `release`, `partial`,
`reserve`) must exit10 at their exact semantic witness. Fixture-only observation
hooks check Type retains before dangerous cleanup and roots before fallible
recursive copying. They are absent from production. Deliberate predicate failures
release genuine native ledger allocations before exit. Compiler/crash/sanitizer/
resource/timeout or unrelated failures never count as controls.

This is bounded native storage/instance-copy GC acceptance, not full production
pool/scheduler, arbitrary Type callbacks, cyclic-array copy, simultaneous mutators,
managed Dis instruction/import/call/frame-root or Android/AWM acceptance. Native
Type sharing does not grant a wire kind, RC uniqueness or execution ownership.
