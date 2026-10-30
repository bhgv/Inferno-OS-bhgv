# Existing exception ownership connected to collector epochs

The standalone process-root and vmachine tests left a separate ownership gate:
actual exception production/selection while the collector observes the graph.
This checkpoint connects production RAISE, ASCII string2c, both emu/os handler
implementations, destroy/freeheap/freeptrs/initmem and the complete collector.
It uses the bounded native arena from gc-roots.c; it is not a full emulator boot
or admission of a new-format mapped Dis64 image.

## Production correction

A real RAISE of an empty String prepares empty error text and retains the value
in Prog.exval. Previously handler returned immediately for empty text, leaving
that owning reference behind. Empty-text terminal behavior still selects no
handler, but now takes the common unhandled cleanup path and releases exval.
This also covers a typed tuple whose message is empty.

For unhandled and final pending-exception cleanup, both ports now capture the
owned value, publish H in the process field, then call destroy on the captured
value. A Type.free callback can no longer observe a process root naming an owner
whose reference count has already reached zero. Existing destroy/gchalt still
protects C cleanup from collector progress; this ordering correction does not
claim that the old callback already permitted concurrent collection.

Selected transfer retains its existing contract: the mapped slot receives the
held value before the process root is removed. A separate reference already in
the slot is released exactly once. Exception PC selection and empty-text terminal
semantics do not change. Nil current process still returns without touching an
owner. No exception matching, wire or calling-ABI IDs are added.

## Actual connected paths and controlled services

`libinterp/tests/exception-gc.py` borrows complete production bodies. The C fixture
creates native root metadata and calls actual RAISE with a genuine native source
slot. Actual string2c validates the ASCII String and prepares its text; actual
RAISE retains exval and transfers through a bounded setjmp error service. The
fixture releases its original source reference, leaving only legitimate process,
frame and graph references before selection. It does not substitute a prepared
process value for the instruction.

Actual handler lookup and selection then run. Its diagnostic realloc boundary
executes six complete production collector epochs before returning allocation
failure. The still process-owned exception must survive these epochs. The
selected handler transfers it into the frame, clears the process root, publishes
live/saved PC and returns. Six more epochs verify that only the mapped frame
keeps the value alive. Actual freeptrs/destroy releases that frame owner; after
the stack root is removed, six epochs reclaim any remaining unreachable cycle.

Payloads are a String or a tuple containing a String and a cyclic child. Five
profiles cover ordinary transfer, cleanup-map transfer, no matching handler,
empty terminal text and an existing slot alias owning a separate reference.
Three initial collector color phases are tested for every profile. The fixture
observes actual destructor callbacks and attempts rungc while gchalt is held:
the slice must increment its halted counter without advancing epoch or cursor.
Root publication is checked inside the actual descriptor-free callback.

Pool release and descriptor reference bookkeeping remain bounded fixture
services. Fresh exception-string allocation, Rune cache allocation, cross-frame
unwinding, allocator compaction and worker scheduling are not exercised here.
The exception tables/frame maps are injected native runtime metadata. They do
not pass through or expand the Dis64 scalar loader. Kernel handler bodies are
compiled and run on the host; this is not a native-kernel boot.

## Evidence

```
python3 libinterp/tests/exception-gc.py
CFLAGS='-O3 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/exception-gc.py
```

Default O1/NDEBUG UBSan and O3/NDEBUG ASan+UBSan pass both handler ports in actual
legacy WORD64 and DIS_WORD32=1/native-pointer64 selections. Each configuration
executes30 graphs and2,204 active predicates:18 selected,12 unhandled,
18 diagnostic collection checkpoints and39 blocked destructor collection
attempts. Totals:120 actual RAISE graphs,8,816 predicates,72 selected transfers,
48 terminal/unmatched cleanup paths,72 diagnostic checkpoints and156 blocked
collector attempts. No predicate is disabled by NDEBUG.

The restricted host reports ASan no-return-stack/symbolization warnings around
the nonlocal error boundary; all four instrumented processes still finish their
active checks without an address/undefined error. The inherited fixture disables
only LSan's unavailable process-inspection scan. This is not warning-free full
host/runtime certification.

The driver is registered in the owning core runner. On the same sources:

- exception-frames.py O3/NDEBUG UBSan:526,000 cases per port/model,2,104,000 total;
- raise-native.py O3/NDEBUG UBSan:actual C and AMD64 complete JIT-tail RAISE,
  both scalar models, existing lifetime/cache and packed-span matrices;
- dis64-vmachine.py O1/NDEBUG UBSan:all eight connected image/model runs and
  both compiled-runtime negative controls;
- word32-build.py:75 complete core objects, native scalar/frame probes and
  invalid build-selector checks;
- native-consumer-layout.py:measured existing blockers again; --require-coherent
  returns2, not readiness. WORD32 C sizes Sys_Dir80 and Connection24 differ
  from generated descriptors64/12; Sys.sleep result16/argument36 differ from
  proposed Frame64 offsets32/64;
- jit-readiness.py --check and Python syntax/diff checks pass.

## Runtime controls

Actual pre-fix handler source from890a12eb4b2fa9066d2fc6b5cc94916abdeba901 compiles,
then the default matrix fails when a callback observes the stale process root.
EXCEPTION_GC_PROFILE=3 isolates the empty-text owner-retention failure. Replacing
only the collector with gc.c from89f7e81 compiles, then fails after collection
inside diagnostic realloc because the sole process exval root was omitted.
These are separate compiled runtime failures, not linker/compiler failures.
EXCEPTION_GC_HANDLER and EXCEPTION_GC_COLLECTOR select the exact alternate files.

Shared Dis64 wire/registry/default targets and43-op scalar/np0 admission stay
unchanged. The [native-consumer audit](dis64-native-consumer-audit.md) remains
an explicit blocker. Calls/dependencies, Frame64 argument/result/temp roots,
real allocator pressure, concurrency, full vmachine-plus-collector scheduling,
complete boot and physical ARM64/RV64 still need independent acceptance.
Complex exception/collection handlers remain C; no backend bytes change here.
