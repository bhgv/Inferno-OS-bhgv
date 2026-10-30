# Process exception roots and packed-frame collection

Two collector gaps remain relevant to the native runtime independently of the
new Dis64 wire format. The process root scan did not mark Prog.exval, although
that field owns the current exception reference. Its frame walk also read
Frame.t/Frame.mr and Type.size/Type.mark through typed pointers, unlike the
existing byte-safe mapped-field and unwinder paths.

The production root scan now marks a non-H exception owner with the ordinary
Setmark barrier. It copies each frame header and descriptor size/callback before
calling the marker. The original frame address is still passed to that marker;
frame order, map semantics, callback order, module roots, native external roots
and reference counts are unchanged. A packed second frame no longer requires
native C alignment. The existing readable/valid root-graph contract remains;
this change is not a probe for arbitrary host memory or malformed stack geometry.

## Complete collector, bounded native arena

`libinterp/tests/gc-roots.py` compiles the complete current gc.c body against the
production header, separately in legacy WORD64 and actual DIS_WORD32=1 models.
It does not mirror rootset or substitute the marker/sweeper algorithm. The pool
adapter supplies eight contiguous blocks with actual Bhdr/Heap geometry and
observes release by marking blocks free. The actual rootset, markheap and rungc
perform color transitions, deferred traversal, epoch completion and sweep.
Reference counts remain nonzero while unreachable cycles are collected.

Exception cases span all eight process states, three initial color phases and
four independent-root profiles: no other root, a module-cache original MP, a
current Modlink MP, or another process's aliased exception root. A two-object
exception cycle survives six complete epochs. Removing only one process alias
preserves it; removing the final exception root reclaims it. Independent MP roots
remain alive until separately removed; unrelated unreachable objects are swept.
Descriptor references return to their initial counts. This tests actual ownership
behavior rather than only checking the exception object's immediate mark color.

Frame cases independently vary the second frame's native alignment0..7, packed
descriptor alignment0..7, and explicit/fallback frame Type:128 cases per model.
They preserve frame/extent bytes, trace native mapped slots and caller Modlink/
MP roots across six epochs, then remove the stack root and verify actual sweep.
The standalone first root remains live. Natural native Heap headers remain a
requirement; only frame/type metadata and mapped pointer slots are packed.

Across both models there are192 exception graphs and256 frame graphs. Default
O1/NDEBUG UBSan and O3/NDEBUG ASan+UBSan each pass26,596 active predicates.
Each model reports6,385 exception and6,913 frame predicates. Address/undefined
checks stay enabled; the fixture disables only LeakSanitizer's process-inspection
scan, which is unavailable on the restricted host. The arena has explicit block,
release, descriptor and retained-root checks; this is not allocator compaction.

```
python3 libinterp/tests/gc-roots.py
CFLAGS='-O3 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/gc-roots.py
```

The driver is registered in the core checkpoint runner. Existing GC arithmetic
regressions pass O3/NDEBUG UBSan in both scalar models, including full gc.c object
compilation. Existing packed-map regressions pass2,961,056 cases across the two
models. JIT readiness retains173 complete rows and166 guarded native paths for
AMD64/ARM64, with WORD32/WORD64 emitter/capacity checks.

## Parent controls

Using the exact gc.c from89f7e8193b1a84e23c0dcbc95de9ab337211c0d9:

- The exception graph test compiles and fails an active retained-root predicate
  after actual collection; this is not a compilation failure.
- The frame test with naturally aligned Type metadata compiles, then UBSan reports
  a misaligned Frame access when the second frame is packed.
- Independently shifted Type metadata reproduces a misaligned Type access.

The same source-selection mechanism can isolate either family:

```
git show 89f7e8193b1a84e23c0dcbc95de9ab337211c0d9:libinterp/gc.c > /tmp/inferno-gc-before.c
GC_ROOT_SOURCE=/tmp/inferno-gc-before.c GC_ROOT_CASE=exception python3 libinterp/tests/gc-roots.py
GC_ROOT_SOURCE=/tmp/inferno-gc-before.c GC_ROOT_CASE=frames GC_ROOT_TYPE_SHIFT=0 python3 libinterp/tests/gc-roots.py
```

## Relationship to Dis64 and remaining work

These are fixes in the existing C collector, not newly enabled Dis64 references.
The new-format loader still accepts43 scalar opcodes and np0 frame/data maps.
No control section, exception opcode, extension/ABI ID, default target or native
call signature is changed. The previous [vmachine gate](dis64-vmachine.md) still
uses a bounded GC boundary; this separate test runs the real collector against
explicit native roots. Combining the two under the real allocator and full
runtime remains a distinct gate.

Live pool compaction, concurrent workers, full emulator boot, generated builtin
interfaces, physical ARM64/RV64 and Frame64 temporary/result-root ownership are
not established here. Complex collection and cleanup remain in C. The next
integration work must preserve exval while transferring it into a mapped frame,
and test caller/result ownership with the collector before enabling the new ABI.

The later [exception/GC checkpoint](dis64-exception-gc.md) connects actual
RAISE/string2c and both handlers to collector epochs before/after mapped-slot
transfer. It fixes empty-text exval retention and clears process roots before
destructor callbacks. The arena and error services remain bounded; no new wire
reference/control admission or complete runtime scheduling is inferred.
