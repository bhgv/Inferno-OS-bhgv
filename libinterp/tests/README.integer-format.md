# Signed integer formatting on AMD64 and ARM64

CVTWC and CVTLC now have guarded native paths that format a captured signed WORD
or LONG into an existing unique ASCII string with capacity at least 24. This
allows every signed 64-bit input, its minus sign and the terminating NUL, matching
the C formatter. Shared, Rune, small and H destinations continue through C.
No new private layout fields or runtime ABI changes are introduced.

Both emitters capture the full input before reading or writing a possibly aliased
output slot. They reject nil/H output owners, negative representation lengths,
length/capacity disagreement, insufficient capacity and nonunique owner counts
without any writes. After these guards, unsigned division by ten emits digits
in reverse order, then a bounded swap loop restores their order. Unsigned
magnitude handles MIN without signed division overflow. Length is published
last. Only the final character span, its NUL and the four-byte length may change;
REG, owner slots, capacity, cached storage, descriptor/heap counts and FP state
remain unchanged. Both leaves fit the existing 64-unit private staging bound.

C CVTWC/CVTLC use the same unique-storage condition. They reject nil outputs,
dead owners, INT_MIN lengths and inconsistent capacity before allocation or
release. Allocation failure preserves the old output; a pending allocation
failure cannot affect eligible unique storage. Capturing input first supports
values inside the output payload or the same input/output slot. Replaying the
parent string.c reproduces dead-owner destruction and unnecessary allocation
failure in the new C-only regression selection.

Validation:

- integer-format-jit.py: 138240 independent image/write/FP checks per backend
  (276480 total), both WORD widths, both opcodes, all eight input/output byte
  alignments, payload and shared-slot aliases, MIN/MAX, signs, digit boundaries,
  dead/shared/saturated counts and malformed headers. Sizing and short output
  capacities are checked against unchanged publication canaries.
- integer-format-native.py: 80640 C/RX cases per WORD model (161280 total) with
  actual production compiler publication and executable AMD64 code. Checks cover
  fallback allocations, unique reuse, exact header/slot/REG images, shared owner
  cleanup, allocation failures and saturated/dead Type counts that need no retain.
  Both models pass ASan/UBSan. FORMAT_C_ONLY and FORMAT_MODE select parent replay.
- integer-format-layouts.py: 371700 sizing/publication/canary probes per backend
  (743400 total) under ASan/UBSan, varying all five relevant private fields.
- leaf64-jit.py guard selection: 1171 instruction/ABI/alias checks across the
  three architectures. No ctypes layout changes are needed.
- leaf64-runtime.py now publishes an 81-instruction module with both new opcodes;
  AMD64, ARM64 and RV64 checks cover target availability, RX publication, invalid
  metadata, allocation/protection failure, retry and unmapping. ARM64 instructions
  execute through Unicorn; host publication tests do not execute ARM64 code.
- The core suite now contains 95 host drivers. Existing string-slot and allocation
  regressions remain included.

Out of 173 active Dis instructions, AMD64/ARM64 now provide 141 guarded native
paths / 32 remaining. RV64 M remains 103/70, Zmmul 90/83, I 87/86. These are
partial guarded paths, not full opcode semantics or a complete native VM ABI.
