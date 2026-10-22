# Initial module-data cloning and failure ownership

Run `python3 libinterp/tests/heap-clones.py` from the repository root.

Each of two scalar models (native WORD and word32 with host-width pointers)
runs 189 cases. The fixture executes production arraycpy/newmp, heap allocation
and initialization, reference acquisition, freeheap/freearray/freeptrs,
linkm/mklinkmod/linkmod/freemodlink and emulator protection/rethrow bodies.

Nested arrays use ordinary user element descriptors, not the global heap-object
Tarray descriptor. Clones must have independent mutable descendants, correct
scalar bytes and retained non-array references. Coverage includes nesting to
five levels, shared source children, nil slots, pointer-bearing record elements,
array slices, empty arrays with nil data, zero-sized elements, and MP pointer
maps spanning two bytes. Pointer-free data retains a bulk-copy path.

Every clone-tree allocation and every module-instance allocation fails in turn.
Partial arrays and MPs must remain safe to destroy, restore all source object
and Type reference counts, and propagate the original VM error. Instance tests
cover failure before a Modlink exists, MP allocation, nested children, unresolved
imports, type mismatch, successful binding and mkmp=0. Injected native-init/end
hooks model owned dynamic data, an allocation failure and a late initializer
error. Each fixture module keeps a separate cache reference, so losing or double
releasing the instance's module reference is observable.

A bounded pool verifies exact payload guards. Final destroy/unload and dynamic
hooks are injected boundaries; destroy invokes the actual array/record cleanup
bodies. This is not a full module unloader, real dynamic-library initialization,
a GC/compaction test, a running Inferno system, or a complete 32-bit ABI build.
Initial Dis data is acyclic and bounded by DADEPTH; this is not a general clone
operation for arbitrary cyclic runtime graphs. newmp constructs separate,
newly allocated destination storage and is not an in-place update operation.
Scheduler/newprog/newstack error exits remain separate integration work.

Strict UBSan is the default. ASan also passes with stack instrumentation enabled:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/heap-clones.py
```

Only unsupported LeakSanitizer process inspection is disabled.
`HEAP_CLONE_SOURCE` selects an alternate heap.c for array copying;
`HEAP_NEWMP_SOURCE` independently overrides newmp;
`LINK_INSTANCE_SOURCE` selects an alternate link.c.
`HEAP_CLONE_CASES=arrays|mp|instances|maps` isolates the families. Original bodies
independently fail nested separation, partial MP ownership and instance module
reference cleanup. Existing heap-bounds and module-links fixtures continue to
exercise their original boundary and lookup regressions.
