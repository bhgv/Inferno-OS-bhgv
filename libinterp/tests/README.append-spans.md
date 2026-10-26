# Checked native string-append spans

ADDC now checks complete source and destination payload spans on AMD64 and ARM64.
A valid character count and capacity do not establish that adding the inline
data offset or byte extent to a native owner address is representable. Each leaf
compares its complete end address with the original owner before copying or
publishing len. Rune byte counts use native-width arithmetic. The combined
character count is bounded by signed destination capacity, so its scaled extent
cannot wrap the native domain more than once.

Both leaves now handle matching ASCII/Rune appends of up to 256 source characters,
including unique self-appends and exact capacities. Mixed representations,
allocation, sharing and unsafe forward-copy overlap retain C. Rejection performs
no writes. Layouts that overlap len/max or place payload inside either field are
rejected during sizing without publishing partial code.

ARM64 combines adjacent signed metadata reads, chains capacity predicates with
CCMP and copies with post-indexed byte accesses. Nonadjacent metadata retains
separate loads. The default fixture is 62 units, and the nonadjacent coverage
layout is 64; AMD64 is 62 units. The 64-unit limit and private ABI are unchanged.

The C addstring handler now checks both existing source spans before shortcuts
or allocation and the extended destination span before reuse. Integer metadata,
reference counts, source lengths and growth rules retain their existing checks.
Overflow raises exHeap before payload access or result publication.

Validation:
- 114,992 emulated append instruction/image/write/FP cases across both backends
  and WORD models, expanded around 255/256/257-character boundaries.
- 3,600 native-address boundary cases, including wrapping inline offsets,
  source ends and destination ends, ASCII/Rune storage and adjacent/nonadjacent
  private metadata. The parent emitters fail the new boundary regression.
- 269,568 production C/published RX AMD64 image/slot/ownership cases, passing
  ASan/UBSan in both WORD models.
- 768 reduced-address C cases under ASan/UBSan. Only the production span-arithmetic
  address type is narrowed; real fixture pointers stay native and accessible.
  These independently check rejection before preparation and complete unchanged
  source/destination images. The parent C handler fails this regression.
- 371,700 sizing/capacity/canary probes under ASan/UBSan.
- 1,171 instruction/ABI checks; the core suite now contains 100 host drivers.

This broadens an existing opcode path. Counts remain 142 guarded native paths
and 31 interpreter-only commands on AMD64/ARM64, out of 173 active Dis commands.
RV64 M/Zmmul/I remains 103/70, 90/83 and 87/86. ARM64 instruction execution is
emulated; physical target and complete VM acceptance remain separate.
