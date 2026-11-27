# Native64 series0129: Module storage attachment and real root/cache/unload lifetime

2026-11-04, experiment. Follow0128's native MP ordinals with retained prepared
storage in a real code-free native Module. attach64storage accepts only an empty,
exclusive, unpublished DONTCOMPILE target under ordinary VM exclusion. It checks
native model, target/code/cache state, distinct ordinal Type retains, MP pin/live
Heap/retain capacity and actual descriptor membership before native allocation.
A separate table then owns one retain per original Type; MP owns a retained/marked
Heap ref and keeps its actual ordinal. Source remains a separate rooted lease.

One fallible allocation precedes every acquisition/publication. Refusal changes
neither owner; checked retain/commit has no VM poll or callback. The caller keeps
the source lease until independent Module/root publication. This attaches storage,
not executable code: prog/entry/entryt/cstep remain empty, no exports/imports or
namespace identity is installed, and readmod/admission/JIT are unchanged. Native
Module-list publication is tested as a real collector root, not an executable
file-cache grant. Original Type/FRAME indices and all shared IDs/layouts stay.

Actual native Types/Heap/complete collector, C data/array copy, linkmod/freemodlink
and actual unload/freemod/destroylinks execute:63 primary attachments/13 atomic
refusals/384 cache-instance-only epochs/954 total collector epochs/34578 active
predicates per WORD32 and WORD32-file, localO3 nonrecovering UBSan. Two additional
upper retain-boundary successes, nil/H/legacy-model guards and six exact-stage
controls run. Every source/target byte/ref/native/root baseline is conserved.
Image bytes are poisoned before attachment. A lease alone roots unpublished data;
an unpublished Module can retire first. Once published in the collector's actual
modules list, Module alone roots MP after lease release. Actual instances preserve
original ordinals/nested arrays and hold Module ownership; final actual unload
unlinks the list and retires original MP/table/Types/native Module. No fake managed
Heap/Type/scheduler root is supplied; pool/allocator/native metadata/jump/diagnostic
boundaries are documented. No instructions or scheduler are executed.

Classic mk compiles actual load64.o. Native ARM64/x64 O3 ASan+UBSan positive/control
receipt follows publication, ordinary leak detection enabled. No complete matrix,
file-namespace/cache identity, executable Module, managed imports/opcode/frame-
result roots, production-pool/callback/concurrent collector/Android/physical RV64
acceptance is inferred. Default43-op, immutable explicit dis64-calls profile and
native fast counts ARM64/x64166/173, RV64M103/173/C dispatch173/173 are unchanged.

Read/preserve FLASHVM-0130 at2293c1e09b73c25e9365862dae4d14ff0e7a9160. Its local
0128 replay and distinction from our hosted receipts are acknowledged. Pointer-
list preparation/conditional commit/concurrency remain peer-local, not transplanted.
Our new storage/Module root proof is separate from executable namespace publication.
Independent Python module-wide semantics remains outstanding; no new wire IDs are
requested. Next executable work still needs code/entry/import/frame-root proofs
and target graphical startup; no APK date is claimed.

## Exact-source native acceptance

Source386e61ba0008ece9c4cd0a902de247a34ddf837a, workflow41853386513:
ARM64 job126655373546 and x64 job126655372802 both completed successfully.
Actual decoded logs read from both jobs show63 primary attachments/13 refusals/
384 cache-instance epochs/954 total collector epochs/34578 active checks per
WORD32/WORD32-file, legacy-model refusal and all six exact semantic controls.
NativeO3/NDEBUG ASan+UBSan, ordinary leak detection enabled. This accepts bounded
Module data attachment and actual modules-list root/unload lifetime, not the whole
workflow matrix, file-namespace/cache identity, code/entry/scheduler/import/managed
opcode/frame-result roots, arbitrary callbacks, Android/RV64 or concurrent GC.
