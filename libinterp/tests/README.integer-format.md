# Signed integer formatting on AMD64 and ARM64

CVTWC and CVTLC format captured signed WORD/LONG values into a unique ASCII or Rune
string whenever its byte capacity fits the actual sign and digits. A
one-character ASCII buffer can therefore format 0; exact capacities through the
20-character signed 64-bit minimum are accepted. The string allocation retains
its existing extra terminator storage. Shared, H and insufficient-capacity
destinations retain C construction paths.

The native counting pass takes an unsigned magnitude and counts sign/digits
without touching the owner, payload, output slot or REG. It validates capacity
before the formatting pass writes anything. The signed input remains captured,
so MIN, payload aliases and shared scalar/owner slots work in both WORD models.
The counting pass fixes the final end address. The formatting pass writes
NUL there and emits digits backwards directly into their final positions,
then publishes the four-byte length last. Rune reuse also publishes converted byte capacity after all guards. Other bytes, heap/Type counters,
caches and FP state remain unchanged. ARM64 shares one rejection return to keep
both passes inside the existing 64-unit staging bound. No layout fields or ABI
changes are introduced.

C stages at most 20 characters and NUL in a 24-byte local buffer, then selects
reuse using the exact length. A pending allocation failure cannot affect an
eligible unique destination, even at the exact capacity. Invalid output owners
and header metadata are still rejected before ownership changes; failed
construction leaves the old result intact. Parent replay with FORMAT_C_ONLY=1
and FORMAT_MODE=15 demonstrates the former needless allocation for a one-byte
destination.

Validation:

- integer-format-jit.py: 627984 independent image/write/FP checks per backend
  (1255968 total). Tests include both WORD widths/opcodes, all eight operand
  alignments, payload/shared-slot aliases, every capacity from 0 through 21,
  MIN/MAX and both signs around every power of ten through 10^18.
- integer-format-native.py: 352440 actual C/RX cases per WORD model (704880
  total), including exact-capacity reuse under forced allocation failure,
  fallback allocations, unchanged header/slot/REG images, shared-owner cleanup
  and saturated/dead Type counts requiring no retain. Both pass ASan/UBSan.
- integer-format-layouts.py: 371700 sizing/publication/canary probes per
  backend (743400 total) under ASan/UBSan, varying five private layout fields.
- Existing 1171 instruction/ABI checks pass across AMD64, ARM64 and RV64.
  Publication/fallback/metadata/failure/retry/unmap checks pass on all three
  backends using an 81-instruction module containing both format commands.
- The core suite now contains 96 host drivers, including the independent
  conversion-source tests described in README.conversion-sources.md.

Out of 173 active Dis commands, AMD64/ARM64 retain 142 guarded native paths /
31 remaining. RV64 M remains 103/70, Zmmul 90/83, I 87/86. These counts describe
partial guarded paths, not complete opcode semantics or a full native VM ABI.
ARM64 instruction execution is emulated; physical target acceptance is separate.

## Reusing Rune allocations

[Reusable Rune formatting](README.rune-format.md) documents the new capacity
conversion, cache ownership and pointer-overflow guards. Rune max is measured
in characters; successful reuse changes it to max*sizeof(Rune) bytes before
publishing the positive ASCII length. Oversized conversions decline or raise
before any mutation. No ABI/layout field or opcode ID is added.

Selected real values now share the decimal writer; see
[README.real-format.md](README.real-format.md).
