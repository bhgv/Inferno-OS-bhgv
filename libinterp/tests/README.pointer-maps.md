# Byte-safe pointer maps and list construction

Latest packed descriptor/GC and native builtin cleanup checks: [README.gc-packed-maps.md](README.gc-packed-maps.md).

Heap pointer-map helpers now access mapped fields through memmove rather than
casting record bytes to WORD**. This permits four-byte or packed alignment on
64-bit native-pointer hosts and avoids forming data pointers for descriptors
with no marked fields. Pointer-map units remain sizeof(void*), exactly as in
the old WORD** iteration; no new Dis frame/GC ABI is selected.

incmem retains references and runs the existing Setmark barrier. scanptrs keeps
ascending field callback order. freeptrs preserves descending slot order within
each map byte and ascending map-byte order. initmem changes only mapped fields
to H, retaining all scalar bytes, padding and surrounding guards. The helpers do
not add descriptor verification or alter the canonical H sentinel.

The cons helper now accepts an untyped destination slot, captures its old tail
with memmove and rejects C-null tails before allocating or publishing a node.
The pointer calling convention is unchanged; existing List** callers convert
normally to void*. Scalar/pointer constructors capture source bytes before
allocation, and consp rejects C-null sources before changing ownership. Raw and
mapped constructors capture counts/type indices and destination owners without
alignment assumptions. Their original staged publication and map reference
retention remain intact. consf preserves raw binary64 patterns, including NaNs.

## Checks

```
python3 libinterp/tests/pointer-maps.py
python3 libinterp/tests/lists.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

The pointer-map fixture executes the four actual production functions at every
byte alignment, for every one-byte mask and three-map-byte variants. It reports
49,176 checks per word model (98,352 total), comparing all surrounding bytes,
reference increments/decrements, mark colors, nprop and exact callback/destructor
order. Marked fields include repeated owners and H; unmarked fields contain
poison bit patterns. Empty/all-zero maps also receive nil/H/unmapped data spans.
WORD32 tests retain native host pointers, matching the other scalar-model
fixtures; this is not a complete 32-bit-pointer process test.

The VM list fixture adds unaligned source/count/index/owner slots, allocation
failure preservation, real child ownership for consp/consmp, raw NaN/zero/infinity
payloads and null-tail/null-source rejection before any allocator call. The base
process reports 3,455 cases per model; nine separate invalid-constructor processes
add one each, giving 3,464 per model (6,928 total). All run with ASan/UBSan and
stack instrumentation enabled. Each invalid constructor is isolated because this
sandbox can leave ASan shadow state after mixed siglongjmp call paths; checks
are not disabled. LSan process inspection remains disabled in these fixtures.
The core suite now runs 28 host drivers and includes the pointer-map fixture.

## Parent-source regressions

HEAP_MAP_SOURCE selects an alternate heap.c. With parent bfa6a60, each of
POINTER_MAP_CASE=inc/free/scan/init fails UBSan on its unaligned pointer load or
store. Current sources pass all four families under ASan/UBSan.

LIST_SOURCE selects an alternate xec.c. LIST_HEAD_CASE=construct reproduces an
unaligned tail-owner load against the parent. null-tail and null-source reproduce
null heap-header arithmetic after allocation/publication in the old constructors.
Current sources raise exNilref before allocation and leave all owners untouched.
The list/heap fixtures match the old cons declaration when substituting that
parent, so declaration differences do not stand in for runtime failures.

These checks cover the changed helpers and list lifetimes; they do not constitute
complete Inferno boots or hardware acceptance of the new 64-bit JIT backends.
Tk, security/cryptography and MIPS/SPARC/PowerPC JIT sources are unchanged.
