# Native64 series0126: owned mapped storage with real collector lifetime

2026-11-04, experiment. Follow0125's structural composition with actual native
storage construction, without misrepresenting an unchecked Module as executable.
The new opaque Dis64StorageOwner API stages copied real Types and module-data;
it cannot be scheduled as Module/Modlink and carries no code/import capability.

Before allocation, prepare64storage checks native model, bounded complete storage
and cross-section facts. The protected constructor adopts each partial allocation,
copies all map8 descriptors using actual dtype, allocates MP with actual heapz,
roots it through real ptradd and applies present scalar DATA with runtime H bytes.
No source pointer survives. Absent DATA is private zero/nil staging only. Release
removes only its acquired root, consumes data, then descriptor/array/native owners.
Ordinary VM exclusion and native metadata allocation without VM collector polls
remain prerequisites; there is no added parallel-mutator/concurrency protocol.

Actual complete collector and heap/protected bodies pass489 independent raw rows,
29 successful preparations,690 structural/allocation refusals,192 collector epochs
and141,707 active checks per WORD32/WORD32-file model at localO3 UBSan. Source is
poisoned before GC; data/maps remain valid. A genuine mapped child cycle survives
under its rooted parent, then is swept after RC parent retirement. An independent
retained alias/root survives removal of the preparation lease. Every constructor
allocation is faulted. Six exact semantic controls and a legacy-model refusal
are included. Native allocations have independent guards and leak ledgers; no
fake Heap or scheduler root is supplied to GC. Pool geometry/native services and
platform jump are bounded, while actual rungc(nil) and emu kprotect execute.

Classic mk compiles actual load64.o successfully. Previous scalar runtime C/RX
regression passes744 images/1,667,800 predicates. Scheduler regression passes
17,774 predicates/12 terminal processes/10 final-exit requests; six native attempts
decline and12 C paths run, so this is not completed JIT-tail execution evidence. Existing scalar fixtures explicitly exclude this distinct
non-executable staging block and retain their original scope. Hosted native ARM64/
x64 O3 ASan+UBSan positive/control acceptance follows publication. No APK claimed.

Read/preserve incoming FLASHVM-0128 at6311ab940b98c13acaebb4cbc9f3f3f1f4b822a0.
Its local0125 replay/review is acknowledged separately from its still-outstanding
independent Python module-wide checker. Requested API clarification is now explicit:
borrowed source/policy/result occupy disjoint independently live mapped storage;
source stays immutable/mapped for every view use. Alias handling is not a new wire
rejection. Peer Heap-hold/parallel runtime code is not imported across heaps.

No shared wire/feature/ABI IDs, Frame64, loader admission or JIT counts change.
Fast ARM64/x64166/173; RV64M103/173; C dispatch173/173. This is actual bounded
storage/GC progress; managed instruction/call/import graphs, frame/result roots,
full Module adoption/retirement and Android/AWM startup still remain.

## Exact-source native acceptance

Source277e4a9fb2350e987ee035453dc875388ba26903, workflow41840242475:
native ARM64 job126609604043 and x64 job126609604408 both completed success.
Actual decoded logs read from both jobs show489 rows/29 constructions/690 refusals/
192 collector epochs/141707 checks for EACH WORD32 and WORD32-file model, valid
legacy-image rejection before allocation, and all six named semantic controls.
Build flagsO3/NDEBUG ASan+UBSan; ordinary leak detection remains enabled. This
accepts actual bounded rooted storage/GC lifetime, not an entire workflow matrix,
full production pool, scheduler/managed execution, arbitrary destructors, concurrent
collector, Android/AWM or physical RV64. No executable admission gate is relaxed.
