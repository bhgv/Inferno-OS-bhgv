# Checked string-comparison address spans

C stringcmp now validates both accessed common-prefix spans before lexicographic
payload reads. It checks the inline data offset before adding the ASCII/Rune
byte count in native pointer width. Capacity and INT_MIN checks still precede
identity shortcuts. Equal owners and empty common prefixes retain their
payload-free behavior; malformed accessed spans raise exHeap without changing
operands, owners or branch PC.

All six AMD64/ARM64 predicates now decline owners in the final 16 GiB of the
native address space before reading their metadata or payload. Language nil
retains its existing empty-string path. The conservative window covers any
supported int-sized Rune payload plus the private inline offset, so accepted
owners cannot wrap during normalization or scanning. Legitimate upper-window
owners remain available through C's precise prefix checks, including identities
and empty comparisons. Ordinary heap-address support is unchanged.

This also prevents a wrapped data pointer from colliding with the nil sentinel
during native identity comparison. False predicates still leave an inaccessible
target untouched. True predicates read and publish the captured full-width
Dis target, including same-value PC writes.

ARM64 shares its Rune stride constant and selects one/four with CSINC after
taking the absolute signed length. A nil length is zero and bypasses payload
loads, so its stride is not needed. AMD64 shares nearby rejection returns and
keeps the upper-window threshold in a temporary register until comparison
starts. Both adjacent/nonadjacent metadata fixtures fit exactly 64 units in
both WORD models; no private ABI or staging-size change is needed.

Validation:
- 79,488 emulated address-window/predicate/image/write/FP cases across AMD64
  and ARM64. They exercise both sides of the exact owner threshold, wrapped
  inline/payload addresses, all representations, signed Rune keys, identities,
  nil/null, empty and long-prefix cases, malformed metadata and poison targets.
  Separate parent-emitter regressions fault while reading wrapped payloads.
- 313,344 reduced-address C cases under ASan/UBSan and NDEBUG cover precise
  prefix spans, identity/error precedence, all slot alignments, inaccessible
  false/error targets and complete unchanged source/operand images.
  Separate ASCII/Rune parent-handler regressions fail.
- 2,230,200 sizing/publication/canary probes under ASan/UBSan.
- 1,666,236 production C/published RX AMD64 Unicode/alignment/PC comparisons
  under ASan/UBSan, including all 65,536 byte pairs and signed Rune boundaries.
- 31,342 instruction/ABI/byte-branch oracle checks and ARM64 compiler
  publication pass. The core checkpoint now has 103 host drivers.

The six existing opcode paths remain partial. Counts stay AMD64/ARM64 142 native
paths / 31 interpreter-only commands out of 173 active commands. RV64 M/Zmmul/I
remains 103/70, 90/83 and 87/86. ARM64 instruction execution is emulated;
physical-target and complete VM acceptance remain separate.
