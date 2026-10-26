# Concatenation preflight before metadata reads

C ADDC and AMD64/ARM64 append leaves validate native pointer slots before loading
their contents. Nil, H and wrapping slot spans cannot reach a memory load or
result publication. C checks all three slots; the native path requires the left
and destination slot to be identical and checks both readable slots.

C addstring validates both len/max header spans and Heap origins before reading
lengths, capacities or references, including when the other input is H. Nil
String inputs still raise exNilref; invalid arithmetic raises exHeap. Valid H
shortcuts, ownership, allocation failure ordering and ASCII/Rune behavior remain
covered by the existing functional matrix.

The native owner predicate checks one contiguous arithmetic range spanning the
backward reference cell and the inline String header. It rejects owner values
at or below the declared reference offset, H, and forward span wrap before the
first owner read. Scratch registers preserve both captured owners. C knows the
full Heap size; the leaf knows its explicit reference-cell offset. These guards
do not prove arbitrary addresses identify mapped allocations.

ADDC's extra preflight exceeds the old 64-unit test capacity. Fixtures now use
the backend's existing 128-unit private buffer and check every insufficient
capacity for zero publication. Production LeafMax, layout ABI and the declared
opcode set are unchanged. There are no new helpers at execution time, stack
frames, allocations, ownership mutations or FP operations on a declined path.

Validation across independent WORD32/64 models:

* `string-append-native.py`: 269,568 C/AMD64 RX image/alias/owner cases plus 374
  preflight cases. The latter include each slot, both owners, low Heap origins,
  top-address header wrap, H shortcuts, unchanged REG/heap images and zero
  allocations. The complete matrix passes ASan/UBSan/NDEBUG.
* `string-append-jit.py`: 115,030 AMD64/ARM64 instruction/image/write/FP and
  sizing/canary checks. Existing ASCII/Rune capacities, full-width references,
  empty input, self-append, aliases and overlap behavior stay covered.
* `append-spans-jit.py`: 6,240 top-address/image/write checks, including 1,840 new
  preflight cases. Read hooks forbid object/reference reads in these rejected
  cases, and write hooks observe even same-value stores. Three private field
  layouts, including nonadjacent fields and a 128-byte reference offset, exercise
  both WORD models.
* `append-span-c.py`: 768 reduced-address source/destination range cases backed
  by actual accessible storage.

Replaying previous AMD64 and ARM64 emitters fails the slot and owner probes
separately. The previous C implementation independently fails the decoded-slot
and owner-header probes. APPEND_PROBE_ONLY/APPEND_PROBE_KIND select instruction
probes; APPEND_PREFLIGHT_ONLY/APPEND_PREFLIGHT_OWNERS_ONLY select native C probes.
All new checks use explicit conditions and remain enabled under NDEBUG/python -O.

Guarded native availability remains 164/173 on AMD64/ARM64; nine commands retain
only the interpreter path. Full Inferno boot and physical ARM64 remain separate
from host execution and instruction emulation.
