# Character access preflight

AMD64/ARM64 INDC and INSC validate all decoded slots before reading operand
values or String metadata. Widths follow the actual operand: native pointers
use eight bytes, while scalar values, indices and INDC results use declared
WORD width. This preserves WORD32/64 alias and misalignment behavior.

INDC bounds the complete inline String header before len/max loads. INSC bounds
the backward reference cell and forward header before reference/length/capacity
reads, preserving its captured Rune and index. Existing representation, capacity,
selected-character, append, unique-reference and overlap rules remain intact.
Declined leaves neither publish a result nor write characters or logical lengths.
The existing 128-unit private capacity and layout ABI remain unchanged.

C mirrors slot/header preflight. INSC additionally validates the complete Heap
origin, whose size is known to C; the native adapter knows its reference-cell
offset. Nil/H owner behavior remains covered by functional tests. These range
checks do not prove arbitrary pointers identify mapped allocations.

The reduced-address C fixture now narrows object geometry only. Decoded operand
slots keep native host addresses. Its independent oracle gives header-wrap
errors priority over index/length-dependent errors, avoiding the field read
that previously preceded the check.

Validation across WORD32/64:

* `string-store-native.py`: 507,904 C/AMD64 RX owner/capacity/alias/FP cases and
  172 new slot/header/Heap-origin cases, plus existing INDC capacity regressions.
  The full matrix passes ASan/UBSan/NDEBUG. New probes preserve complete REG,
  slot and heap images and perform no VM allocations on rejection.
* `python3 -O string-store-jit.py`: 336,214 actual AMD64/ARM64 instruction,
  owner/capacity/alias/FP and sizing/canary checks. Assertions in this driver,
  the native matrix and the reused allocator/EXPECT fixture are replaced by
  explicit predicates, including normalization of native pointer conditions.
* `python3 -O string-cell-spans-jit.py`: 13,776 INDC/INSC and adjacent SLICEC
  instruction/image/write cases. This includes 656 new preflight probes across
  both metadata layouts. Read hooks forbid String/reference reads on rejected
  slot/owner probes; write hooks observe same-value stores too.
* `string-cell-spans-c.py`: 14,520 reduced-address extent/image/ownership cases
  backed by real accessible allocations, including shared replacement storage.

Previous emitters separately fail slot and owner probes for both INDC and INSC
on each architecture. Previous C bodies separately fail decoded-slot and owner
probes. CELL_PROBE_ONLY, CELL_PROBE_KINDS and CELL_PROBE_FAMILY select instruction
replays; CELL_PREFLIGHT_ONLY and CELL_OWNER_PROBE_ONLY select native C probes.

Measured guarded native availability remains 164/173 on AMD64/ARM64. This series
improves existing opcode paths; it does not claim full operand coverage, complete
NC64 ABI acceptance, physical ARM64 execution or a full Inferno boot.
