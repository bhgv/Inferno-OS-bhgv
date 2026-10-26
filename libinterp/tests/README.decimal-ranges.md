# Full signed decimal ranges and checked UTF-8 views

AMD64/ARM64 CVTCW and CVTCL now accept the complete signed result range when the
ASCII digit sequence has at most 10 digits for WORD32 or 19 digits for a 64-bit
result. An optional leading sign and bounded leading zeros are supported.
Empty strings, sign-only integer strings and language nil retain zero results.
Whitespace, trailing text, Rune strings, longer sequences and out-of-range
magnitudes retain C parsing. CVTCF keeps its existing exact 15-digit domain and
negative-zero behavior.

At most 19 decimal digits fit an unsigned 64-bit accumulator. The integer
leaves compare the final magnitude with signed_max + negative before negation,
including the extra magnitude needed for signed_min. This admits both extrema
without signed overflow or loss of the full WORD width.

All three leaves now validate the owner-relative span through the trailing
NUL before reading it or scanning digits. The existing terminator must still
be zero; otherwise C performs string2c's observable termination write.
Metadata layouts cannot overlap len/max or place data inside either field.
Native rejection remains write-free and preserves FP state and errno.

C string2c checks the ASCII payload plus its written NUL, or the full Rune
source span, before access. Rune checks precede cache sizing/allocation and
mutation. Rejected spans retain the old UTF-8 cache. Existing logical-capacity,
UTF byte-count and allocation-error precedence is preserved.

Both adjacent and nonadjacent metadata fixtures remain within 64 units, without
private ABI changes:

| Opcode | AMD64 WORD32 | AMD64 WORD64 | ARM64 WORD32/64 |
| --- | ---: | ---: | ---: |
| CVTCW | 48 | 49 | 57 |
| CVTCL | 49 | 49 | 57 |
| CVTCF | 49 | 49 | 56 |

Validation:
- 892,377 emulated decimal instruction/image/alias/layout/FP checks, including
  every byte in an interior digit position and randomized signed digit strings.
  All Python checks remain active with PYTHONOPTIMIZE=1.
- 13,800 top-address cases across both architectures and WORD models, checking
  inline-offset/NUL-span overflow, signed limits, aliases and FP state.
  Separate parent regressions fail for all three opcode paths on both backends.
- 1,115,100 sizing/capacity/canary probes under ASan/UBSan.
- 2,543,616 C/published RX AMD64 comparisons against host parsers under
  ASan/UBSan and NDEBUG, covering full ranges, overflow, aliases, four rounding
  modes, preexisting sticky flags and errno.
- 2,202,624 additional comparisons with actual lib9/strtoll.c and libmath/dtoa.c
  under ASan/UBSan and NDEBUG. Real comparisons select the exact native domain;
  big comparisons also cover C fallback. A Python-integer table independently
  models lib9's modulo-2^64 overflow, including unsigned-maximum boundaries.
  Overflow remains in C, preserving the production parser's behavior.
- 840 reduced-address C UTF-8 view cases check native spans, ASCII termination,
  cache preservation and allocation/error ordering. Separate ASCII and Rune
  regressions fail on the parent handler.
- 1,171 instruction/ABI checks, ARM64 compiler publication and 102 core host
  drivers pass.

Production-parser comparison command:

    DECIMAL_VM_PARSER=1 DECIMAL_VM_BIG_PARSER=1 DECIMAL_ACCEPTED_ONLY=1 \
    CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' \
    python3 libinterp/tests/decimal-native.py

Counts remain 142 guarded native paths / 31 interpreter-only commands on both
AMD64 and ARM64, out of 173 active commands. RV64 M/Zmmul/I remains 103/70,
90/83 and 87/86. These are partial paths, not complete operand coverage.
ARM64 execution is emulated; physical target and full VM acceptance remain separate.
