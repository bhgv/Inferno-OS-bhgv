# Module construction roots across allocation and cloning

linkmod previously kept its newly allocated Modlink only in LinkModule.result on
the C stack. Allocation of the new MP, nested array copying and dynamic native
initialization could run the collector before a caller frame owned that instance.
The instance could be swept during its own construction, invoking freemodlink
and unloading the input Module prematurely.

There is a separate source lifetime boundary: an input Module that is not yet
published in the module list does not by itself root its original MP. That MP
could be collected during the first allocation, before the instance even exists.
Holding an ordinary Module reference is distinct from a collector heap root.

The constructor now temporarily pins the original MP when it will clone that
MP and pins the partial Modlink before subsequent fallible operations. Each pin
has its own success flag in the outer protected context. A failed acquisition
never removes another root. Successful return removes both temporary roots.
Failure removes successful roots before destroying the partial instance or
unloading the input Module, preserving the existing single-consumption contract.

The caller still must publish/root the returned instance and maintain normal
Module publication/lifetime. Construction pins do not make an unpublished Module
permanently visible to the collector. The source remains borrowed from its
Module; adding a temporary root does not acquire a separate reference-count owner.
Root-metadata allocation retains the existing non-collecting single-owner
assumption. Arbitrary throwing destructors are outside this checkpoint.

## Actual heap/collector gate

utils/tests/native64-module-roots.py compiles complete actual link.c, heap.c and
math.c objects with the complete gc.c collector body. It reuses generated frames
and bounded pool services, increasing this fixture's arena from8 to12 blocks.
The source MP owns nested arrays. Every heap allocation during actual linkmod
construction runs six actual collector epochs.

Two profiles exercise a cached input Module and an input not yet published.
After construction the latter is explicitly published by the fixture before
further collection. The returned instance moves into a generated caller map.
Every process state Palt..Pbroken and three initial colors are exercised.
Copies remain independent, all actual temporary-root buckets are empty on return,
post-publication collection retains both graphs, and real destruction releases
the instance, cloned MP and arrays. The module-owner boundary observes exactly
one actual freemodlink callback per completed instance. Final Type owners balance.

Across two compatible model selections the gate runs96 constructions and16130
active predicates. The complete native64-layout gate supplies its fresh owning
compiler to this driver, including its previous actual GC and protected-result
checks. This is a bounded collector gate, not full scheduler/worker integration.
Module cache unload and unused dynamic native initialization are controlled
boundaries; complete newdyndata or arbitrary dynamic modules are not certified.

NATIVE64_LINK_SOURCE selects a parent source; NATIVE64_MODULE_CASE=cached or
unpublished isolates the two profiles. Original link.c at a2ca32b compiles and
links with the same gate, then reaches an active predicate/SIGABRT. The cached
profile fails after36 checks on an unexpected unload during construction; the
unpublished profile fails after37 checks when original MP storage is reclaimed.
The latter stops before reused storage is written. These controls do not depend
on compilation failure, a sanitizer report or an invalid-pointer crash.

## Error and compatibility checks

The existing actual recursive clone/error fixture now fails all6 source,
instance and nested-array pin acquisitions of its module construction graph.
For every failure it requires the original object/Type reference counts,
module ownership, zero temporary roots and outer error-frame depth to be
restored. Together with previous array/module cases it passes210 cases per
WORD model (420 total) under O1/strict UBSan and O3/NDEBUG/strict UBSan.
Its bounded external-root service checks ownership, rather than implementing GC.
The module-data preflight fixture retains its17 cases/model but now places a
real Heap owner header before its borrowed payload, as required by root services.

The actual positive module/heap/collector gate also passes O3 ASan+UBSan for
both models. Only unavailable LeakSanitizer process inspection is disabled.
Prior optimized recursive error-unwind ASan failures remain unaccepted; this
positive gate is not an ASan acceptance of arbitrary VM unwind or callbacks.

```
python3 utils/tests/native64-layout.py
NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-layout.py
NATIVE64_LIMBO=/path/to/matched/limbo NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' python3 utils/tests/native64-module-roots.py
python3 libinterp/tests/heap-clones.py
```

No new wire IDs, signatures, native opcode or runtime registration is assigned.
The fix applies to ordinary C module construction used by every backend. Complex
cloning and allocation remain in C. Dis64 file admission remains43 scalar opcodes
with np0; reference admission, shared call/unwind ownership, async task lifetime
and default NC64/SDK registration are unchanged.
