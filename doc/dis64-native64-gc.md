# Generated native64 frames with actual C consumers and collector roots

This checkpoint connects generated map8 to the ordinary runtime Type descriptor,
actual heap operations, selected Math C callbacks and the complete collector.
It is private opt-in evidence, not native module registration or a shared
reference-call signature/ownership agreement. The43-op np0 file profile and
general inspector execution_supported=false remain unchanged.

## Descriptor adapter

After checking generated geometry and successful private table binding, include
native64-frame.h and call native64frametype(record). The helper checks the
WORD32/ref64 model and the complete fixed-frame record before calling actual
dtype with freeheap. The returned ordinary Type owns a copied map and has the
normal markheap callback. Release it with freetype after frames stop borrowing
it. Generated maps/records remain immutable and are not owned by this descriptor.

A malformed16-byte map for the1264-byte/20-map-byte frame is rejected before
allocation. Missing callbacks are rejected. A descriptor-allocation failure
returns nil. A real20-byte descriptor retains its suffix independently of the
source map; a reference at argument149/offset1256 survives actual collection.
The model/geometry/binding preconditions are still necessary: this helper is
not a general wire parser or structural type checker.

## Actual source closure

utils/tests/native64-gc.py uses the existing bounded pool/stack test services
from gc-roots.c, but includes the complete production gc.c body. It compiles
complete actual heap.c and math.c objects. The old fixture freetype service is
removed: descriptor creation/release, allocation, initialization, array copy,
reference retain/release, destruction and sweeping use the actual heap source.
No numerical implementation or production function body is extracted/replaced.

The pool has eight aligned blocks and deliberately retains freed bytes for
observation. Pool allocation runs six real collector epochs before allocating
the new block. It does not collect inside an uninitialized Heap header. Allocation
failure is injected only at the descriptor allocator boundary. kprotect executes
a valid callback directly; error unwind/partial-construction recovery is not
proved here. Module/channel services outside the graph fail actively if reached,
including when sanitizer global registration retains their canonical metadata.
This is a bounded single-owner graph, not full VM boot/compaction/concurrency.

## Argument, temporary and caller roots

For every process state Palt..Pbroken and each of three initial color phases:

1. Generated Math import/export frame maps become actual Type descriptors.
   The bound actual Math_import_int/Math_export_int callbacks process real byte
   and WORD arrays, preserving signed boundary values and big-endian encoding.
   Six collector epochs before/between/after calls retain arguments solely via
   frame maps; ordinary actual freeptrs/initmem cleanup releases both owners.
2. Actual arraycpy returns an array sharing a non-array child with a self-cycle.
   This avoids the distinct recursive nested-array cloning contract. Generated
   Roots.copy and Roots.caller frames have different extents and maps:
   callee72/map80 versus caller88/map60 (hex map bytes after the empty header
   byte). The result address at32 points to the caller slot at72. A scalar big
   at64 retains its canary across cleanup, and a separate byte-array owner at80
   must survive collection and be released with the result. Cleanup uses the
   actual caller descriptor, never the callee descriptor.
   Existing actual ptradd/ptrdel temporarily pin the returned C owner. The source
   frame owner is removed and destroyed; collection must retain the result and
   its child via the temporary root. After slot publication/unpin, collection
   must retain them via the caller map alone.
3. Repeat the result transfer with an existing caller owner. The new pointer is
   published before destroying the old owner. A destructor observer delegates
   to actual freearray and attempts actual rungc while gchalt is set: it observes
   the new caller slot and the correctly blocked collector, not a completed
   concurrent collection. Six epochs after destruction exercise the new root.
   Dropping the result releases its array, and actual sweeping finally reclaims
   the now-unreachable child self-cycle.

Roots.copy/caller callback symbols are fixture metadata anchors, not new runtime
builtins. The actual reference producer is arraycpy; the fixture performs the
explicit pin/publication/unpin sequence through the generated result address.
It does not claim an existing reference-returning builtin was registered/called
through a new Dis64 ABI. Math calls are actual selected builtin implementations.

Across two compatible models each run executes150 graphs,96 actual Math calls,
48 old-owner destructor checkpoints and14548 active predicates. Each model also
runs six independent controls: remove argument roots, omit the temporary pin,
remove the caller roots, truncate the wide descriptor to16 map bytes, use the
wrong callee descriptor for caller cleanup, remove only the separate tail root. Every
control first compiles/links the same actual sources and must terminate at its
active runtime root predicate with SIGABRT. Compile errors are not negative
controls. No heap payload is dereferenced after its failed liveness predicate.

## Verification and remaining boundaries

```
python3 utils/tests/native64-layout.py
NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-layout.py
NATIVE64_LIMBO=/path/to/matched/limbo NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' python3 utils/tests/native64-gc.py
```

The layout suite supplies its fresh owning compiler to the GC driver. The
standalone driver can build a compiler itself or use an explicitly supplied one.
The ASan pass uses a matched source-built compiler without claiming that compiler
itself was rebuilt under ASan. Actual heap/Math/collector/probes are instrumented;
only unavailable process-inspection leak scans are disabled by the existing
fixture hook. Restricted-host executable-name/symbolization warnings may appear;
no warning-free host or whole-engine sanitizer certification is claimed.

The experiment demonstrates a root-transfer sequence using existing runtime
mechanisms. It assigns no wire IDs, changes no default SDK or Runtab registration,
and does not settle structural reference signatures, variadic tails, automatic
header-temporary roots, error/unwind pin cleanup, nested allocating partial trees,
full scheduler/collector integration or parallel workers. Those need separate
agreement and tests before reference-call admission. Linux AMD64 is the executed
host; incoming FlashVM QEMU ARM64/RV64 reports are separate peer evidence.

The next [protected C result checkpoint](dis64-native64-result.md) exercises
producer errors and pin-allocation failure with actual port kprotect/rethrow
bodies, preserving the old owner and original error during cleanup.
