# Cycle-safe list lengths on C, AMD64, ARM64 and RV64

LENL now rejects cyclic lists with a VM heap error before publishing a result.
The C walker uses an unsigned count and rejects the first count beyond the
positive WORD range before incrementing. Nil links retain nil-reference errors;
H terminates an ordinary list and yields its full valid length.

Brent checkpoints are retained at counts 1, 3, 7, ... . Comparing the next link
with that checkpoint detects cycles without allocations or additional link
reads. The checkpoint schedule uses count & (count+1); the count bound ensures
that this unsigned addition is representable. No partial count, reference,
collector state or operand slot is published on error. This checks graph
structure, not every arbitrary inaccessible pointer or complete heap lifetime.

AMD64/ARM64/RV64 use the same checkpoint schedule inside a bounded native scan.
The bound increases from 128 to 256 nodes, with exactly one link read per visited
node and one final result publication for successful walks. Short cycles decline
early; longer/malformed graphs return to C without writes. Valid lists longer
than 256 still return their complete C length. Private layouts/ABIs and the
64-unit staging bound are unchanged. RV64 uses only base integer operations.

Validation:

- list-length-jit.py: 34560 independent visited-set graph/image/read/write/FP
  checks per backend (103680 total), both WORD widths, lengths through 513,
  nil/H terminals, back edges at the start/middle/end, all input/output byte
  alignments, owner/result aliases and result slots inside link storage. Link
  read hooks check the bounded scan and early rejection of short cycles.
  RISC-V FP status is seeded/read through real CSR instructions.
- list-length-native.py: 17280 actual C/RX cases per WORD model (34560 total),
  including valid long-list fallback, cycle/nil error identity, unchanged
  rejected images and FP state. Both models pass ASan/UBSan. An additional
  130-case reduced 8-bit arithmetic model reaches the exact positive count
  limit and first rejected counts; it is not an added VM ABI.
- list-length-layouts.py: 370170 sizing/capacity/canary probes per backend
  (1110510 total), under ASan/UBSan. RV tests retain the emitter's existing
  bounded partial-prefix contract for undersized output buffers.
- Legacy list-length selection: 1438 cross-backend checks; guard selection:
  1171 instruction/ABI checks; original RV32/RV64 suite: 2174 checks. All
  compiler publication/fallback/failure/retry/unmap fixtures pass.
- Parent replay with LIST_LENGTH_CYCLE=1 times out on a one-node cycle, while
  LIST_LENGTH_COUNTER_ONLY=1 exposes the unchecked count limit. Both pass on
  the fixed source. LIST_LENGTH_SOURCE selects a replay interpreter source.

The core suite now has 98 host drivers. Opcode availability remains AMD64/ARM64
141 guarded commands / 32 remaining; RV64 M 103/70, Zmmul 90/83 and I 87/86
out of 173 active commands. ARM64/RV64 instruction execution is emulated.
