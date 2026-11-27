# Native64 series0127: independent data clones with real C array and GC semantics

2026-11-04, experiment. Extend0126's opaque rooted storage preparation with a
separately releasable clone. This supplies native instance-data ownership before
future executable Module adoption, without accepting structurally framed code.

clone64storage retains descriptor identities at their original ordinals, reserving
both a table retain and the actual MP Type's extra heapz retain before allocation.
It uses the MP Heap's actual type rather than assuming ordinal0. A new independently
owned Type-pointer array and nil-initialized MP are adopted immediately; the MP is
rooted before fallible newmp/arraycpy C work. Original data/source roots remain
independent. Protected failure destroys every partial mapped descendant and
retained descriptor/root. Success remains opaque storage, never Module/Modlink,
imports, namespace/cache or JIT permission. Caller holds existing VM exclusion and
keeps source live/unchanged; no parallel-mutator mechanism is introduced.

Actual newmp/arraycpy/copyarray/heaparray/initarray/freearray, heap and complete
collector execute on genuine allocated objects.33 graph cases:66 graph clones,
131 allocation refusals,2 body refusals,513 collector epochs/31993 active checks
per WORD32/WORD32-file, localO3 UBSan. Additional retain-boundary/source guards and
legacy-model refusal pass. Native/data/Type/root baselines and guard bytes restore.
Source bytes are poisoned after preparation. GC polls at every descendant pool
allocation preserve the original/new/intermediate array roots. Two nested arrays
are independent copies while ordinary leaf refs share/retain. A clone lives after
its original lease retires. ULONG_MAX leaf retains throw inside actual ordinary/
nested C copy and unwind protected partial ownership. MP descriptor ordinals0,1,64
and65-type tables work without renumbering. No production copying algorithm is
replaced. Six exact-stage controls verify retain/pin/copy/release/partial/reserve.

Classic mk compiles actual load64.o. The0126 storage-owner fixture still passes
489 inputs/29 preparations/690 refusals/192 epochs/141707 checks per model plus
legacy refusal. Its extraction explicitly separates the new clone API, whose
actual bodies/collector are accepted by the new fixture. Native ARM64/x64 hosted
O3 ASan+UBSan positive/control receipt follows publication. No complete-matrix,
Android, scheduler, cyclic-array copy or concurrent collector claim is made.

Direct Dis64 coordination remains via the two existing journals. Shared wire/
feature/ABI IDs, Frame64, immutable typed25 verdicts7/12/4/2, default43-op file
admission and explicit immutable dis64-calls profile remain unchanged. Native fast
JIT ARM64/x64166/173 and RV64M103/173, C dispatch173/173. Next: actual executable
Module adoption, managed instruction/import graphs, frame/result roots and AWM
startup on target; this source does not claim an APK or a delivery date.

## Exact-source native acceptance

Source972bb657d9fc085c97ed00e3eea77c2b8b04b657, workflow41844279660:
ARM64 job126623598067 and x64 job126623597620 both completed successfully.
Actual decoded logs from both jobs show33 graphs/66 graph clones/131 allocation
refusals/2 body refusals/513 collector epochs/31993 checks for each WORD32 and
WORD32-file model, legacy-model refusal and all six semantic controls. Native
O3/NDEBUG ASan+UBSan, ordinary leak detection enabled. This is bounded actual C
copy/GC/ownership acceptance, not an entire workflow matrix, executable Module,
managed opcode/import/frame-root, production pool/scheduler, cyclic-array copy,
concurrent collector, Android/AWM or physical RV64 acceptance. No gate is relaxed.
