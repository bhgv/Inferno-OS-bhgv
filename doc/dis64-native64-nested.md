# Keep allocating array clones rooted during construction

A production arraycpy result used to remain only in ArrayCopy.result on the C
stack while newmp recursively cloned nested arrays. The collector does not scan
that stack. Collection during a descendant allocation could therefore reclaim
the partially initialized parent. Ordinary allocation-failure tests without
collection did not detect the missing root.

heap.c now pins a mapped destination before descending into its elements.
ArrayCopy records successful pin acquisition separately from result ownership.
A pin allocation failure leaves result owned for cleanup without attempting to
remove a pin that was never acquired. Successful return and error cleanup each
remove their own successful pin. Error cleanup then destroys the complete
partial tree before the existing krethrow. Scalar-array copying needs no pin
because no descendant allocation follows its allocation.

This uses the existing ptradd/ptrdel protocol and retains its synchronous,
single-owner assumption: root-metadata allocation does not collect an unpinned
new owner. It does not assign a shared reference signature, change module
registration or prove arbitrary destructor callbacks cannot throw.

## Actual collector regression

utils/tests/native64-nested.py builds complete production heap.c and math.c
objects and the complete gc.c collector body. It reuses bounded pool/stack
services from native64-gc.py and the generated Frame64 argument map. Three
acyclic nested arrays contain a byte payload; each descendant allocation runs
six actual collector epochs. The source is retained through its actual frame,
and the partial copy must be retained only through the new temporary roots.

Across every process state Palt..Pbroken and three initial collector colors,
cloned arrays must survive, have independent identities and contents, and permit
mutation without changing the source. Successful return leaves every actual
external-root bucket empty. Destruction releases all six source/copy arrays and
restores canonical Type owners. Two model selections execute48 deep clones and
6336 active predicates per run. The full native64-layout driver supplies its
fresh owning compiler to this additional gate.

The original heap.c at ee39013 compiles/links with the same fixture and fails at
an active GC predicate after46 checks: an already reclaimed partial-copy block
would otherwise be reused. The fixture refuses that reuse before writing the
new object. This is a production lifetime regression, unlike the previous
equal-map fixture improvement. NATIVE64_HEAP_SOURCE selects the parent body for
independent reproduction; no removed-root stub substitutes for the actual GC.

## Error and compatibility gates

The existing heap-clones fixture retains complete actual emulator
kprotect/krethrow bodies and actual recursive clone/free functions, with bounded
allocator and external-root services. It now rejects unmatched pin releases and
requires zero pins when snapshots are restored. It injects failure at all15
mapped-array pin acquisitions in a depth-four clone, in addition to every
existing heap allocation failure. The current suite also covers module construction pins:210 cases pass per WORD
model (420 total) under O1/strict UBSan and O3/NDEBUG/strict UBSan. See the
[module construction checkpoint](dis64-native64-module-roots.md). These services prove balanced
ownership across unwind; they are not a second real collector implementation.
Heap-bounds-derived fixtures supply matching bounded root services as well.

The actual nested collector gate passes O3 ASan+UBSan for both models. A separate
optimized ASan run of recursive error-unwind heap-clones is NOT accepted: the
restricted host warns that __asan_handle_no_return lacks stack bounds, then
reports stack-buffer-overflow/shadow errors after siglongjmp. Repeating with
-fno-inline -fno-omit-frame-pointer did not resolve this run. This observation is
not proof that the report is a false positive. Address/UB checks are not disabled
to obtain an unwind acceptance. Existing protected-result ASan limitations remain.
Only unavailable LeakSanitizer process inspection is disabled in the fixtures.

```
python3 utils/tests/native64-layout.py
NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-layout.py
NATIVE64_LIMBO=/path/to/matched/limbo NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' python3 utils/tests/native64-nested.py
python3 libinterp/tests/heap-clones.py
```

This fixes ordinary deep-array construction for existing runtime callers on all
backends. It adds no native opcode, Dis64 reference admission or async lifetime
contract. Cyclic nested-array cloning, arbitrary partial C producers, full
scheduler/concurrent collection and physical-target execution remain outside
this gate. See also [generated frame roots](dis64-native64-gc.md) and
[protected C result ownership](dis64-native64-result.md).
