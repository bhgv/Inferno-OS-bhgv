# Packed metadata and bounded native-code publication

Run `EMIT_LAYOUT_CASE=views python3 libinterp/tests/emitter-layouts.py` from the
repository root. Both AMD64/ARM64 and both WORD models run by default.
`EMIT_LAYOUT_ARCH` and `EMIT_LAYOUT_WORD` select one architecture and WORD width.
`EMIT_LAYOUT_VIEW_CASE` selects output, bridge, layout, bounds or alias. The
additional wrap/bridgewrap selections isolate aligned wrapping destinations;
canonical prints deterministic native and complete-tail images for comparison.

## Changes

The priority leaf and complete-tail encoders formerly published code through
native `u32int` stores. A caller's packed output buffer therefore caused C
alignment UB, including when the emitted target was AMD64. Invalid H/wrapping
output identities were not checked before publication. Leaf encoding also read
LeafLayout fields directly, causing alignment UB for packed metadata and invalid
reads for nil/H/wrapping metadata identities.

The shared leaf layout helper now checks a complete native input span and copies
it into aligned private storage before field access. This snapshot also makes
input/output overlap explicit. Existing layout checks and opcode selection then
operate on the snapshot without changing the borrowed source.

The shared publication helper checks the actual emitted count and complete
native destination span before copying any bytes. `out == nil` remains a sizing
query; `cap` remains capacity in four-byte code units, not a requirement to touch
all of that capacity. Successful publication copies the existing private staged
bytes with memmove, supporting packed intermediate output buffers. The compiler
still allocates and publishes its final executable mappings through the existing
RW-to-RX path. Packed intermediate output support does not permit unaligned ARM64
instruction entry points; the four-byte target guards remain in place.

These checks validate numeric identities, complete spans and C access alignment.
They do not establish memory mapping or allocation ownership for arbitrary
nonwrapping pointer values. Ordinary source and destination allocations must
still be valid for their declared/read/written extents. No opcode semantics or
native instruction encodings are changed.

## Evidence

| Family | Scenarios per architecture/WORD/profile |
| --- | ---: |
| Fast-code output offsets, capacity rejection and wrapped destinations | 5,976 |
| Complete C-tail output offsets, capacity rejection and wrapped destinations | 6,228 |
| All opcode layouts at eight byte alignments | 1,384 |
| Nil/H/wrapping metadata and zero/negative capacities | 1,038 |
| Packed input/output overlaps at 64 offset combinations | 10,624 |
| Total | 25,250 |

Every successful buffer is compared in full with the canonical code and surrounding
canaries. Rejected capacity calls retain the complete caller buffer. Layout input
images remain unchanged unless intentionally overlapped by the output view;
overlap checks compare the exact combined final image. Output capacity INT_MAX
is also used with ordinary small buffers to verify that only actual code bytes
are copied. Nil output queries retain deterministic sizes.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass under NDEBUG: 303,000 scenarios
across AMD64/ARM64 and WORD32/WORD64. The optimized profile also treats native
pointer/int conversion diagnostics as errors. LSan process inspection is disabled
while the selected address/undefined instrumentation remains active.

Twelve parent-header executions against 4f54d7f reproduce sanitizer failures:
packed fast output, packed complete-tail output, packed metadata, nil metadata,
aligned wrapping fast output and aligned wrapping complete-tail output, separately
for AMD64 and ARM64 at WORD64. Each replay must fail with an actual sanitizer
runtime diagnostic; compile errors are not counted as regression evidence.

Canonical snapshots from parent and current headers match byte-for-byte across
692 opcode/architecture/WORD pairs, including both guarded native code and complete
C-tail images. Runtime dispatch also passes through real RX AMD64 code:
177,152 route, 6,183 compiler and 295 integration cases per WORD model. The ARM64
compiler fixture passes 6,215 preflight cases per WORD model on the AMD64 host;
it does not execute ARM64 guest instructions. Existing callback-layout regressions
pass 7,266 cases per architecture/WORD model. Native bootstrap and readiness pass;
bootstrap builds real host tools and compiles all 17 Limbo units.

Readiness is unchanged: AMD64/ARM64 each have 173/173 complete JIT+C entries,
166/173 guarded native fast paths and seven C-only entries. These host checks do
not establish a full booted 64-bit VM or physical ARM64 execution. Tk,
security/crypto and MIPS/SPARC/PowerPC JIT sources remain unchanged.
