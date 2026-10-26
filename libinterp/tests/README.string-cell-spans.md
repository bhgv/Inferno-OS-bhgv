# Checked string cell and slice addresses

INDC, INSC and SLICEC now validate their accessed native byte extents in C and
the AMD64/ARM64 leaves. A valid character index does not establish that adding
the inline data offset or a Rune-scaled index to the owner is representable.
Emitters compare the complete selected end against the original owner before
reading, writing or publishing length. The validated character geometry bounds
the total offset below 2^34, so a wrapped native sum cannot masquerade as a
nonwrapped end. Rejection is write-free.

C INDC checks the selected cell after existing representation/capacity/bounds
validation. C INSC checks that cell before reuse; replacements instead validate
the complete old payload before allocation or widening/copying. Errors leave
the owner, result slot and allocation counters unchanged.

C slicer validates nonempty source spans before allocation. Its common bounds
and empty-result checks now precede both representation branches. In-place
SLICEC checks the selected source end only when moving characters. A zero-start
unique prefix needs no payload access and retains its length-only path, even
for large prefixes. Empty and shared slices retain C handling.

The native moving-slice limit increases from 128 to 256 characters for both
ASCII and Rune strings. Prefixes remain independent of this copy limit.
The fixed staging limit and private ABI are unchanged. Adjacent and nonadjacent
metadata fixtures have these sizes, in four-byte units:

| Opcode | AMD64 WORD32/64 | ARM64 WORD32 | ARM64 WORD64 |
| --- | ---: | ---: | ---: |
| INDC | 35 | 39 | 39 |
| INSC | 49 | 53 | 53 |
| SLICEC | 51 | 52 | 50 |

Validation:
- 13,120 emulated native-address boundary cases across both architectures and
  WORD models, including wrapped inline offsets, character ends and slice ends.
  They cover both metadata layouts, ASCII/Rune storage, prefix-only updates,
  append cells and full-width indices. Each of the three old opcode paths fails
  its separate parent-emitter regression on both architectures.
- 14,520 reduced-address production C cases under ASan/UBSan. Only arithmetic
  address types are narrowed; actual pointers and allocations stay native.
  INDC, unique/shared INSC, in-place SLICEC and allocating slicer each fail
  separate parent-handler regressions.
- 1,115,100 sizing/capacity/canary probes under ASan/UBSan.
- 336,136 existing store emulation checks and 67,584 expanded slice emulation
  checks compare complete images, write traces, aliases and FP state.
- 507,904 C/RX AMD64 store checks and 8,384 C/RX slice checks under ASan/UBSan,
  including new 127/128/129/255/256/257/384-character moving-slice cases.
- 6,814 instruction/ABI/character-oracle checks, ARM64 compiler publication and
  the core suite with 101 host drivers.
- The compiler/runtime exception-folding fixture now shares ASCII/Rune payload
  storage and supplies the native address type used by the real slicer. Its
  10,446 value/error comparisons pass across both WORD models. Its explicit
  failure checks also remain active with NDEBUG; no assert is used in the fixture.

Counts remain AMD64/ARM64 142 guarded native paths / 31 interpreter-only commands
out of 173 active commands. RV64 M/Zmmul/I remains 103/70, 90/83 and 87/86.
These are partial native paths, not complete operand coverage. ARM64 instruction
execution is emulated; physical target and complete VM acceptance remain separate.
