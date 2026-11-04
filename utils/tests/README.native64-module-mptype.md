# Native Module data descriptor ordinals and actual instance GC

Module.mptype is a host-native descriptor ordinal. Official ordinary Dis/module
constructors zero their Module, preserving index0. A trusted native Dis64 builder
can select another checked ordinal without moving Type identities or changing
FRAME indices. This is not a new serialized field/feature/ABI ID; the native
Module size changes and all native consumers must be rebuilt coherently.

Production linkmodule now bounds the ordinal/table before selecting a Type. For
non-nil original MP it checks a live actual Heap and exact descriptor identity
before any source pin/instance/data allocation. A different descriptor's size/map
cannot be used to read or retain the original data. Native source type/map/retain
preflight, source/instance temporary roots, partial cleanup and the historical
nil-source/zero-byte special case remain. Ordinary Dis keeps index0. The ARM32/
386 publication drivers use the same explicit ordinal for their staged MP extent;
MIPS/SPARC/PowerPC backends are not edited.

Run `python3 utils/tests/native64-module-mptype.py`. The fixture executes actual
linkm/mklinkmod/linkmodule/linkmod, freemodlink, heap/newmp and complete collector
bodies, emu kprotect/krethrow. Native Module/table metadata and unload accounting
are explicit service boundaries; every managed object is made by real heap code.
There is no fabricated Heap/Type or scheduler root. Genuine dtype table ordinals
0,1,64 select16-byte mapped data beside unrelated80-byte descriptors. Actual
Modlink/MP creation shares a real child with exact retains, copies scalar bits,
and survives actual GC. Collector epochs may run at every pool allocation while
construction source/instance roots are checked before unsafe continuation.

20 cases/17 refusals/20 native unloads/54 actual collector epochs/16061 active
predicates pass per WORD32 and WORD32-file model locally atO3 nonrecovering UBSan.
Every constructor native/heap/root allocation is refused. Negative/out-of-range/
missing ordinals, mismatched actual Heap descriptor and dead source ownership
refuse before allocation. Source bytes/child/Type/native/root baselines restore.
The caller separately pins an unpublished original after linkmod returns; no
implicit native C Module-cache root is assumed. A real instance root then keeps
its independent MP and shared child alive until actual freemodlink retirement.

Four exact-stage controls (`ordinal`, `descriptor`, `source`, `instance`) must
exit10 at their named semantic predicate. Fixture-only context/effect hooks expose
missing guards/roots before allocation or GC can continue unsafely. Deliberate
failures release genuine native ledger storage. Compiler/crash/sanitizer/resource/
timeout or unrelated failures cannot count. Hosted ARM64/x64 useO3/NDEBUG ASan+
UBSan with ordinary leak detection enabled. Old ARM32/386 publication regressions
and17 module-data retain cases are separately scoped; they are not completed
ARM32/x86 instruction execution proof or new nonzero-ordinal JIT acceptance.

This accepts bounded native instance/data ownership, not the complete module
cache/unload/scheduler, imports, executable Dis64 Module construction, managed
opcodes/frame/result roots, physical device/Android/RV64 or concurrent collection.
No structural image is admitted as executable, and default43-op/immutable named
Dis64 file policy remains unchanged.
