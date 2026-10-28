# Buffered input pushback and position boundaries

Run `python3 libbio/tests/boundaries.py` from the repository root.
`BIO_BOUND_CASE=byte|rune|offset|buffered|seek` isolates each family.
BIO_BOUND_BGETC, BIO_BOUND_BGETRUNE, BIO_BOUND_BOFFSET, BIO_BOUND_BBUFFERED and
BIO_BOUND_BSEEK select alternate production files for independent replay.

Five public routines now handle boundary failures explicitly:

- Bungetc refuses a rewind outside the buffer plus its reserved Bungetsize
  prefix, a positive input cursor, or an unrepresentable int cursor.
- Bungetrune validates the remembered rune width and its complete rewind span.
  Zero width retains the historical successful no-op for an active reader.
  Both rewind functions change EOF to active only after every guard succeeds;
  refusals preserve the complete Biobuf image, including runesize and state.
- Boffset promotes output buffer counts before adding them, then checks signed
  vlong bounds before adding the native position. Unrepresentable positions
  return Beof without mutating the buffer.
- Bbuffered uses widened negation/addition and returns Beof for counts outside
  the public nonnegative int range. Inactive/unknown-state behavior is retained.
- Relative read-side Bseek stops on a failed Boffset query, before changing the
  buffer or calling native seek. Boffset's historical Beof/-1 sentinel remains
  ambiguous with a negative logical position; relative seeking from that error
  sentinel is now refused, as expected for invalid file-position state.

The pushback bound assumes a properly initialized Biobuf and its allocated
reserve. It does not establish mapping ownership for arbitrary fabricated bbuf/
ebuf fields, track whether a caller previously consumed a particular byte, or
promise that bytes ungot before the start of the stream are initialized history.
The fix prevents repeated ungets from escaping the actual reserve of an ordinary
initialized buffer. Unsupported native I/O and allocation ownership are unchanged.

The real production Bgetc replays every accepted nonempty pushback; unexpected
refills fail the oracle. Tests cover sizes 1..32, all normal/adjacent states,
every nearby cursor and rune width, extreme int counts and the int result limit.
Position results use independent __int128 arithmetic for vlong boundaries and
both unread/pending-output counts. Complete Biobuf snapshots enforce no mutation
on query failure and no native callbacks on failed relative position queries.

| Family | Cases |
| --- | ---: |
| Byte pushback | 4,244 |
| Rune pushback | 29,684 |
| Logical positions | 1,323 |
| Buffered counts | 405 |
| Relative seek query failure | 2 |
| Total | 35,658 |

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass with NDEBUG: 106,974 cases in
three profiles. Explicit predicates remain active; LSan process inspection is
disabled while the selected address/undefined instrumentation remains enabled.
Five isolated old-source probes against c1494cd fail on the intended predicate or
UBSan arithmetic diagnostic. Compile failures are not accepted as regression
proof. In the seek probe only Bseek is old, retaining the corrected offset query
to isolate the missing error propagation independently.

Adjacent tests pass: 110 buffered UTF cases, 85 seek scenarios, 96 lifecycle
scenarios, 25,731 Brdstr cases and 2,162 output-state cases. Native bootstrap
builds the host tools, regenerates the real grammar and compiles all 17 Limbo C
units. Incoming c1494cd shared-DAG inline caching also passes its small/deep/DAG
fixtures. The new Bio test is included in the interpreter checkpoint runner.

No JIT production source changes are included. Readiness validation still reports
173/173 complete native+C entries and 166/173 guarded native paths on AMD64/ARM64.
That is entry availability, not full 64-bit VM boot/ABI acceptance. Tk,
security/crypto and MIPS/SPARC/PowerPC JIT sources are untouched.
