# Bio and descriptor formatted-output state

Run `python3 libbio/tests/print-state.py` from the repository root. This links
real lib9 dofmt, string/integer dispatch, UTF support and format registration
with production Bprint/Bvprint/Bflush, fmtfdinit/fmtfdflush and vfprint/
_fmtFdFlush. Only native writes and process error text are modeled.

The series fixes connected formatted-output boundaries:

- Bvprint rejects readers, EOF/inactive buffers, invalid output geometry and
  nil/H format identities before formatter callbacks or buffer writes. This
  matches the existing Bwrite/Bputc output-state contract.
- Bio formatter flushes require a matching current stop and a cursor inside the
  active buffer. Final cursor publication uses the same checks. If Bflush fails,
  its compacted unwritten suffix, free-space count, inactive state and accepted
  native byte offset survive the outer formatting error.
- Bflush and _fmtFdFlush retain the native long write result until it has been
  checked against the requested remainder. Oversized results are rejected before
  pointer/count/offset advancement; no accepted-byte interpretation is invented.
- fmtfdinit validates non-null, nonwrapping storage and nonnegative size before
  changing the caller's Fmt. Valid zero-sized buffers retain their old behavior.
- _fmtFdFlush rejects rune-mode, unordered, null and oversized byte views before
  pointer subtraction or narrowing to its int request domain.
- vfprint rejects nil/H format identities before dispatch and checks initialization.
  Its ordinary short-write completion and final-flush failure contract remains.

Checks establish numeric geometry of caller-provided views, not allocation or
mapping ownership for arbitrary fabricated addresses. Normal formatter callbacks
must honor their buffer contract; output already produced before a later error
is not rolled back. Late state/cursor changes are refused at publication/flush
boundaries, not treated as a general sandbox for malicious formatter code.

## Real-formatter evidence

| Family | AMD64 native-long cases |
| --- | ---: |
| Bio read/inactive-state rejection | 68 |
| Bio complete-stream, short-write and partial-failure matrix | 152,880 |
| Bio invalid initial layout/format | 11 |
| Bio late cursor/state and native-count refusal | 7 |
| Descriptor initialization | 39 |
| Descriptor flush geometry and native-count refusal | 11 |
| Descriptor formatting/internal/final flush | 602 |
| Total | 153,618 |

The stream matrix covers capacities 1..32, every pending prefix, payloads through
96 bytes, three native fragment sizes and seven error thresholds. It compares
accepted and pending bytes against an independent complete stream, including the
exact unwritten suffix after partial failure. Buffer canaries, offsets and counts
are checked with explicit predicates active under NDEBUG. Descriptor formatting
also crosses its real 256-byte private buffer with payloads through 600 bytes.
Oversized-return models include request+1 and 2^32+1 on native-long-64 hosts;
the latter must be rejected before an int conversion could turn it into one byte.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass: 460,854 cases on this AMD64 host.
The two native-long-64 edge cases are conditional on sizeof(long)>4; this is not
a physical ARM64 run. LSan process inspection is disabled; selected address/
undefined instrumentation and static-storage image accounting remain active.

BIO_PRINT_CASE selects state, flush, layout, cursor, fdinit, fdflush or fdformat.
BIO_PRINT_BVPRINT/BPRINT/BFLUSH/FMTFD/VFPRINT select alternate production files.
BIO_PRINT_FDFLUSH_CASE=geometry|oversize isolates descriptor preflight/reply checks.
Nine old-source selections against 6a307e5 reproduce predicate failures or ASan/
UBSan diagnostics: four Bio formatting families, fd initialization, fd flush,
fd formatting, a descriptor oversized-result probe and a Bio native-count probe.
Each changes only the selected production file; compile failures are not counted.

Adjacent tests pass: descriptor I/O 13,923 cases in each native/Android-log model,
formatter arithmetic 144, native output spans 918 plus five nil checks, Bio output
2,162, previous Bio boundary/seek/lifecycle regressions, and native bootstrap.
The bootstrap builds real host tools, regenerates the grammar and compiles all
17 Limbo units. The new formatter driver is registered in the interpreter suite.

No JIT production code, Tk, security/crypto or MIPS/SPARC/PowerPC JIT changes are
included. AMD64/ARM64 readiness still measures 173/173 complete native+C entries
and 166/173 guarded native fast paths; this does not establish full VM boot or
coordinated 64-bit compiler/loader/frame/GC ABI acceptance.
