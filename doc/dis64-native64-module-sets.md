# Atomic native64 builtin module sets

builtinmods64 accepts an immutable array of Native64ModuleSpec requests and a
disjoint caller-owned nil Module* output array. It validates all module names,
generated declarations and selected fixed C entries before allocation. Duplicate
module names inside the set and aggregate declaration/selection counts above 4096
are refused. Bounds are private C construction limits, not Dis64 wire limits.
The zero-count case performs no allocation/publication on a compatible model.
Incompatible/packed models refuse before inspecting request/output contents.

Every module is built through the actual newmod/dtype/runtime machinery while
completed owners remain private. A later constructor failure retires the partial
current module and earlier staged modules in reverse order. Existing registry
links and output cells remain unchanged, and teardown restores the original
failure diagnostic. Cleanup callbacks must not throw or reenter registration.
Ordinary native allocation follows its existing nil-on-failure contract.

Only when all modules exist does a nonallocating/noncallback commit link the
complete chain and publish one new registry head. Outputs remain in request order;
the registry order matches sequential registration, with the last request first.
Pointers use the ordinary builtinmod64 ownership convention: publication owns one
module reference and outputs are aliases, not additional retains. Existing module
name replacement policy is unchanged; the duplicate prohibition is within this
new set. Callers serialize registration and retain readable immutable names/maps/
records through return; pointer bounds do not authenticate arbitrary C memory.

builtinmod64 now shares the set construction/commit engine using stack storage
for its one staged pointer. Its ABI and exact allocation sequence are preserved;
there is no additional scratch allocation for existing single-module callers.
Thus actual Math registration and all existing fixed-frame gates exercise the
same refactored engine, not a parallel replacement implementation.

This transaction covers the module graph only. It does not run module initializers
or undo initializer side effects, Sys global Type initialization, formatter
installation, external I/O or shared libraries. Inputs must already describe a
coherent trusted ABI. It provides the publication boundary for a future complete
native64 builtin set; it does not by itself enable full startup, packed Sys tails,
wire callable signatures, reference profiles or a wider Dis64 admission policy.

## Reproduction

```
NATIVE64_LIMBO=/path/to/fresh/limbo python3 utils/tests/native64-builtins.py --sets
```

The set gate keeps actual Type/module/link constructors and destructors and real
Math C implementations, with a bounded native allocation/process diagnostic
boundary. For each WORD32 and WORD32-file model, 60 set transactions cover every
one of 24 allocation failures in a three-module set, success, previously published
module isolation, unchanged outputs, owned names/maps and unloading a middle
module without damaging its peers. It observes 654 allocator entries: no new
module or output may be visible during construction. Actual scalar/Math links
are invoked after commit; the wide module retains a full 20-byte root map.

Preflight rejects duplicate names, malformed last requests, invalid counts,
occupied outputs and wrapping/null spans without allocation. A generated 4096-name
specification checks the exact aggregate bound and later overflow; the empty
selected module at the bound still constructs and unloads normally. Incompatible
host/packed models reject deliberately unreadable request pointers before use.
Two compiled controls must abort: premature registry publication and omitted
retirement of previously completed staged modules. Native retirement deliberately
changes error text to prove diagnostic preservation.

The ordinary single-module gate retains its exact fault counts and all real 68
Math exports are covered by the complete layout integration. Tests do not establish
moving collection, arbitrary pointer provenance or concurrent registration.
Prior optimized UTF/injected-unwind ASan evidence remains open independently.
