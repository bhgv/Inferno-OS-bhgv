# Descriptor indexing, aggregate slots and channel ownership

This series changes twenty VM opcode handlers: indx/indb/indw/indl/indf, lena,
slicea/slicela, six string comparison branches, movp/tcmp/self and
newcm/isend/irecv. It also repairs their newc/rawchannel construction paths.

## Native descriptor indexing

IINDX now has a WORD64 leaf on x86-64, ARM64 and RV64M. Compiler adapters pass
Array.t and Type.size offsets explicitly. The leaf checks owner and type nil/H,
negative length/stride, complete index bounds, the signed-int byte extent and
native address carry. For positive widths it first bounds the index to INT_MAX,
then multiplies by the nonnegative int stride (product fits 62 bits), and checks
the resulting byte offset. Zero-width elements preserve the backing pointer
identity even for nil/H storage or indices larger than INT_MAX. No backing data
is dereferenced. WORD32 and RV32/no-M models retain C dispatch.

ARM64 and RV64 emission fit the existing 64-instruction limit without changing
the leaf calling convention. No native stack, helper calls, FP operations or GC
maps are introduced. `LEAF64_CASE=descriptor leaf64-jit.py` passes 14,038
encoder/execution/ABI/alias/oracle checks across three targets and both word
models. `LEAF64_CASE=frameabi` passes 712 layout/capacity checks including malformed
new fields and unchanged rejected output. The RISC-V regression passes 2,081
checks. Unicorn/Capstone must be on PYTHONPATH.

`descriptor-index.py` extracts actual array helpers and executes 85,248 valid
stride/alignment/alias/empty cases per word model. The production compiler harness
publishes the new opcode and executes the WORD64 matrix on native x86-64 against
actual C handlers. WORD32 asserts that the native entry is absent. Both host word
models pass ASan/UBSan with the earlier arithmetic/lifetime matrices. ARM64/RV64M
host modes verify publication and cleanup; RV64M capability is simulated for
compilation and target code execution uses Unicorn.

## VM aggregates and references

All five indexing opcodes capture pointer/index slots before an overlapping
store. The array length opcode similarly captures/publishes unaligned values.
`array-indices.py` runs 10,247,040 WORD64 and 10,239,360 WORD32 bounds/address/
alignment/alias checks under ASan/UBSan. Bounds failures leave all bytes unchanged.
WORD32 retains its legacy pointer-to-word narrowing semantics.

Slices now check descriptor validity, byte-product limits and address wrap before
allocation, reference updates or copying. Bounds and owner-pointer slots use
memmove. Empty/zero-sized storage keeps its identity; mapped copies retain their
source owner and use integer address ordering for overlap. `heap-bounds.py` adds
12,800 view/copy/alignment and 8,192 rejected geometry cases per word model,
passing ASan/UBSan in HEAP_CASE=slice-slots and slice-spans. Existing borrowed,
mapped, nil and empty-view tests remain active in the core suite.

`string-branches.py` runs 307,200 cases per word model through actual stringcmp
and all six branch handlers, against a code-point oracle. ASCII/Rune layouts,
VM nil, empty/prefix/order pairs, independent byte alignments and inaccessible
false targets are included. Both models pass ASan/UBSan.

`reference-slots.py` runs 27,456 cases per word model for actual movp/tcmp/self,
checking exact increment/decrement and marking deltas, owner/type guards, exact
aliases and independent byte alignment. Both models pass ASan/UBSan. Pointer
loads/stores no longer require host alignment. Invalid C-null owners are rejected before unsafe heap arithmetic; tcmp retains
its H wildcard behavior.

## Channel construction and transfers

Raw channel construction previously read its width through R.s after newc had
published the replacement and destroyed the old channel. If R.s pointed to the
old channel's mid.w, this was a heap use-after-free. RawChannel now captures the
width before construction; newc also returns its captured result rather than
rereading a destination that destruction may invalidate. The parent fa2366c
reproduces ASan heap-use-after-free in CHANNEL_CREATE_CASE=owner. The corrected
33 owner-replacement cases per word model pass ASan/UBSan.

Channel capacities, owner slots and raw sizes are captured byte-safely before
publication. CHANNEL_CREATE_CASE=alignment passes 6,144 independent source/
capacity/result alignment cases per model under ASan/UBSan. The default channels
driver now tests both WORD models under UBSan, including existing allocation and
rethrow failure injection. The complete injected-failure ASan run is not claimed:
it reports the environment's longjmp/stack-shadow diagnostic; successful new
ownership/alignment groups run independently with stack checking retained.

Existing send/receive transfer scenarios now vary channel/value slot alignment
and compare surrounding bytes. ASan/UBSan passes 140,462 WORD64 and 140,458 WORD32
cases per scheduler, including actual movp ownership changes. The blocking queue
group additionally tests 32 nil/H unaligned channel owners per combination and
asserts no register/data/queue side effects before the VM exception.

The core environment suite passes 42 host drivers, including channel construction
and all three new drivers. Source override variables reproduce unaligned WORD,
String and owner loads on parent fa2366c; HEAP_XEC_SOURCE additionally reproduces
a null-Type member access for malformed slice geometry. Stack instrumentation
is retained; unsupported leak-process inspection is disabled in fixtures.

Hardware ARM64/RV64 execution and complete Inferno boots remain untested. This
remains a decoded-operation leaf JIT; native frames/calls/returns and canonical
Dis64 ABI migration remain separate. Excluded backends and subsystems are unchanged.
