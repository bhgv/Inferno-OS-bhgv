# Complete pushback windows and native refill result widths

Run `python3 libbio/tests/refill-windows.py` from the repository root.
BIO_REFILL_CASE selects line, string, getc, read, rdline, rdstr or write.
BIO_REFILL_BGETC/BGETRUNE/BREAD/BRDLINE/BRDSTR/BWRITE select production files;
BIO_REFILL_REPLY_INDEX isolates a native return-value pattern.

## Ordinary data-loss correction

An initialized Bio buffer has reserved history before bbuf. After valid ungets,
its unread window can exceed bsize without exceeding that allocated reserve.
For example, a one-byte buffer can retain two previously read bytes after two
legal ungets. Previously, with no delimiter in that window:

- Brdline replaced the full unread count with bsize, losing the beginning when
  callers drained Blinelen bytes through Bread.
- Brdstr appended from bbuf instead of the true unread start, dropping the prefix.

Brdline now returns the complete available window length and retains its cursor
and good-data start. Brdstr appends the entire initial window from its actual
address before refilling. The existing delimiter/stripping and allocated-owner
contracts remain. In particular, Blinelen can describe a valid reserved-prefix
window larger than bsize; Bread can consume those exact bytes.

The two ordinary regressions use real production Bgetc/Bungetc/Bread and line
routines, followed by actual 2/3/4-byte Bgetrune/Bungetrune transitions. Short native reads fill the real reserve with known prior history;
legal ungets then cross refill boundaries. All callbacks obey their byte-count
contract in these tests. No malformed Biobuf or fabricated native reply is
needed to reproduce either lost prefix.

## Native result-domain correction

Bgetc, Bread, Brdline, Brdstr and Bwrite retain long native replies until checking
positive results against the exact request remainder. Only bounded counts enter
int cursors or byte-copy lengths. Nonpositive line-reader results are normalized
before narrowing, so a wide negative error cannot become a positive byte count.
Oversized positive replies make the buffer inactive without advancing its tracked
offset from the unsupported result. Existing cached prefixes remain consumable;
line/string readers do not issue further native refills after that terminal state.
Bwrite retains the unwritten suffix after a valid partial write and forces
inactive state for an oversized reply even if stale error text says interrupted.
Ordinary interruption/EOF/error policies otherwise remain unchanged.

These response checks do not undo external I/O or constrain what an erroneous
native callback itself wrote. The synthetic bad-reply models deliberately accept
or write no extra bytes; checks ensure the library does not invent acceptance,
use an unsupported copy length or repeatedly refill a terminal buffer. Properly
initialized buffer geometry and allocated reserve remain caller preconditions.

| Family | Cases per profile |
| --- | ---: |
| Brdline after real refills/ungets | 5,112 |
| Brdstr after real refills/ungets | 5,112 |
| Bgetc reply widths | 256 |
| Bread reply widths | 256 |
| Brdline replies/cached-prefix recovery | 752 |
| Brdstr replies/cached-prefix recovery | 752 |
| Bwrite partial-prefix/native replies | 4,224 |
| Total | 16,464 |

The stream matrix covers capacities 1..16, every short-read fragment size, every
legal remembered-history rewind, delimiters, stripped delimiters and EOF without
a delimiter. Byte images, complete output and storage canaries are checked.
Reply matrices cover capacities 1..32, request+1, LONG_MAX, zero, ordinary and
wide negative errors, and 2^32/2^32+1 where native long supports them. Every output
prefix and both ordinary/interrupted error texts are exercised. Cached input
prefixes survive terminal invalid replies with no further native callback.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass: 49,392 cases. Explicit predicates
remain active under NDEBUG. LSan process inspection is disabled while selected
sanitizer instrumentation stays active. Seven isolated old-source selections
against a42ca20 reproduce failures: two ordinary prefix losses plus one native
width probe per changed I/O routine. Compile failures are not regression proof.

Adjacent rune, moving-allocation Brdstr, output-state, seek, lifecycle, prior
boundary and real-formatter tests pass. Native bootstrap builds the host tools,
regenerates the grammar and compiles all 17 Limbo units. This fixture is included
in the default interpreter checkpoint runner.

JIT production code is unchanged. AMD64/ARM64 readiness remains 173/173 complete
native+C entries and 166/173 guarded native fast paths; full VM boot and the
coordinated 64-bit compiler/loader/frame/GC ABI remain separate acceptance work.
Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources are untouched.
