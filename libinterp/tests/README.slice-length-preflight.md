# String slice and length preflight

AMD64/ARM64 SLICEC validates both scalar geometry slots and the native owner slot
before loading operands. Before metadata reads it bounds the backward reference
cell and the exact len/max header extent. Negative or greater-than-INT_MAX end
values decline before any owner read, matching C geometry error ordering.
Captured start/end values survive the
owner check. C slicec validates slots and header/Heap origins; allocating slicer
validates metadata before reading length/capacity/reference fields.

A zero-start prefix reads no character data. Its preflight therefore bounds only
metadata, preserving prefix reuse when the unused payload base would wrap. Moving
slices still validate their full selected source span before the first write.
Existing copy limits, signed Rune lengths, ownership, errors for invalid geometry,
empty/shared fallback and publication behavior remain covered by the test matrix.

LENC on AMD64/ARM64 and C now validates source and result slots before reading a
String. Existing length-field-only checks remain intact: capacity and character
storage are not required for a length query, and H retains its zero result.

These arithmetic checks do not prove arbitrary pointers identify mapped owners.
Native leaves know explicit reference offsets; C also knows the complete Heap
origin. The backend's existing 128-unit private capacity and layout ABI stay
unchanged. Slice instruction fixtures use that capacity and retain insufficient
capacity/canary checks.

Validation across independent WORD32/64 models with native host pointers:

* `string-slice-native.py`: 8,384 functional C/AMD64 RX slice/copy cases, existing
  rejected-owner/geometry cases and real PROT_NONE-backed huge-prefix checks,
  plus 202 new slot/header/Heap-origin cases for SLICEC, LENC and direct slicer.
  The complete matrix passes ASan/UBSan/NDEBUG. Predicates from the reused
  allocator/EXPECT fixture remain active under NDEBUG and normalize pointer
  conditions explicitly. Rejection preserves REG, captured images and VM
  allocation counts.
* `python3 -O string-slice-jit.py`: 67,584 AMD64/ARM64 complete image/write/FP
  cases plus sizing and canary checks.
* `python3 -O string-cell-spans-jit.py`: 14,768 INDC/INSC/SLICEC/LENC boundary,
  image and write checks. Added cases cover slice/length slots, wrapped headers,
  reference-cell underflow, length-only top-address queries and payload-free
  prefixes. Fifty-six additional invalid-geometry probes reject negative/large
  ends before metadata access. Read hooks forbid premature String/reference reads on rejection.
* `LEAF64_CASE=string python3 -O leaf64-jit.py`: 1,438 instruction/ABI/alias/oracle
  checks, including existing AMD64/ARM64 and unchanged RV64 length paths.
* `string-cell-spans-c.py`: 14,520 independent reduced-address geometry/image/
  ownership cases. The oracle retains basic SLICEC geometry error ordering while
  checking headers before errors that would require a metadata read.

Replaying the version before the early end-range guard independently detects
premature metadata reads for invalid geometry on both backends.
Previous SLICEC emitters independently fail slot and owner probes on each
architecture; previous LENC emitters fail slot probes. Previous C handlers fail
slot probes, while slicec and direct slicer also fail owner-header probes.
SLICE_PREFLIGHT_ONLY, SLICE_OWNER_PROBE_ONLY and SLICE_PROBE_OPERATION select
native C replays; CELL_PROBE_ONLY/FAMILY/KINDS select instruction replays.

Guarded opcode counts remain AMD64/ARM64 164/173. These are partial native paths;
full Inferno build/boot, complete NC64 ABI acceptance and physical ARM64 execution
remain separate from host tests and instruction emulation.
