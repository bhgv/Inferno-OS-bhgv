# Unique string slices and constructor retention

AMD64 and ARM64 now implement `SLICEC` for nonempty slices of uniquely owned
ASCII and Rune strings. The C instruction uses the same storage reuse rule.
Shared owners still allocate independent storage; empty slices still go through
the interpreter so its destruction and nil publication remain authoritative.
The public `slicer` helper continues to allocate and never mutates its input.

Both native paths capture complete WORD bounds and the destination owner before
any write, validate the signed representation against capacity, require exactly
one reference, and bound copied results to 128 characters. Zero-start prefixes update only
the length and are not subject to that copy-loop limit. Rune byte arithmetic
uses native registers after character bounds validation. Forward byte copies
preserve arbitrary 32-bit Rune values and handle overlapping source/destination
ranges. A zero start skips copying. The owner pointer, capacity, heap metadata
and cached-conversion buffer are retained; string2c rebuilds its Rune view on
its next call. Longer slices that move data retain C fallback, where no native loop
limit applies. Leaves retain the existing 64-unit staging limit.

The string allocator had two direct `Tstring.ref++` paths outside the heap
constructors fixed previously. `newstring` and `newrunes` now reject dead,
negative or saturated descriptor counts before allocating. The final legal
retain from INT_MAX-1 remains valid. Both VM and public-helper slicing reject
a dead owner before allocation or destruction, including empty slices.
`stringdup` now rejects that owner state before allocating a replacement.

Validation:

- `string-slice-native.py`: 3,520 production C / published RX AMD64 cases for
  each of WORD32/native-pointer64 and the existing WORD64 model. Checks include
  shared-owner preservation, exact scalar slot boundaries, allocation-free
  unique slices, malformed operands, empty slices, dead-owner rejection and
  saturated descriptor failure before allocation. Selected ASan/UBSan runs pass.
- `string-slice-jit.py`: 23,552 independent image/write/FP checks per backend,
  covering both WORD widths, unaligned operands, operands inside the copied
  payload, ASCII/Rune data, INT_MIN metadata, large bounds, native-width
  reference counts, the 128/129-character boundary and write-free fallback.
  It also checks sizing, malformed layouts and bounded publication. Requires
  Unicorn and Capstone.
- `string-slots.py`: 79,391 existing checks per WORD model pass.
- All 84 default host drivers pass (80 with --core exclusions) across the initial and resumed runs. The
  compiler trap-folding fixture now supplies a live owner count to the real
  slice helper. Native publication/fallback/metadata/retry/unmap tests pass.
- Independently removing descriptor preflight, slice-owner validation or
  duplicate-owner validation makes the regression driver fail.

Current guarded native opcode availability, out of 173 active Dis instructions:

| Backend | With native path | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 134 | 39 |
| ARM64 | 134 | 39 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

Counts apply to both tested WORD models. They count partial guarded opcode
paths, not complete operand coverage or a complete native VM ABI. ARM64 is
checked by instruction emulation; physical ARM64 execution remains outstanding.
