# UTF string formatting, precision and quote widths

Run `python3 lib9/tests/utf-format.py`. The driver links actual byte/Rune
formatter dispatch, UTF helpers and quote formatting from lib9 and libkern,
using lib9 fallback units where the kernel library has no separate copy.
Python generates 140,580 independent vectors per library from valid and malformed
UTF-8, whitespace and apostrophes. Expected Unicode scalars come from Python's
strict decoder, with native one-byte Runeerror recovery. The oracle separately
models the existing byte precision of %s and Rune precision of %S; %q/%Q ignore
precision. Width counts characters for byte and Rune output alike.

Each vector verifies byte output, Rune output, literal text after the conversion,
left/right alignment and every short output-buffer boundary. Short output must
be NUL terminated, preserve its trailing guard and contain complete UTF-8.
Forced quoting and replacement characters must be counted by their output
encoding width, separately from consumed input bytes. Additional cases exercise
bad counted spans, empty nil spans, an exactly sized unterminated input byte,
and streaming flushes with completely filled byte/Rune chunks. A two-byte,
one-scalar allocation with a claimed count of two must report failure without
reading a third byte; ASan reproduces that heap overread in the original body.

Set `UTF_FORMAT_WIDE=1` to select actual 2,147,483,681-byte and Rune inputs on
LP64. Repeated shared 2 MiB mappings plus a private final terminator page avoid
large physical allocations; MAP_FIXED replaces only pages inside one owned
reservation. Both public copy helpers must reject over-wide unbounded input
before narrowing into int counts; explicit small precision must still work.
This mode replaces the ordinary vector run. It is best run with UBSan, while
the ordinary vectors support both ASan and UBSan through CFLAGS.

`UTF_FORMAT_DOFMT` and `UTF_FORMAT_FMTQUOTE` select earlier sources for negative
probes. To test an old dofmt.c alongside the new quote formatter, append the
current bounded _fmtdecode helper to that temporary source: this adapter supplies
only the newly shared symbol and leaves the old copy/precision bodies intact.
`UTF_FORMAT_VALID_ONLY=1` excludes malformed input; `UTF_FORMAT_SKIP_SPANS=1`
skips the private metadata checks when isolating a previous public behavior.

These native-library tests do not boot Inferno, invoke the complete Limbo
compiler or execute native JIT code. Formatter public count fields remain int;
this change reports unrepresentable unbounded string spans rather than changing
the ABI or implementing a new 64-bit JIT backend.
