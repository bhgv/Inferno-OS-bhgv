# Private typed native64 formatter

The shared runt.c parser now reads through xprinttake. Its public xprint wrapper
still selects legacy packed frame alignment/strides. Explicit
DIS_NATIVE64_FORMATTER includes native64-format.h after that shared core, adding
an internal native64format entry which reads only a constructed Native64Arguments
context. No Sys variadic registration or file-call admission is enabled.

A fresh cursor is created for each formatting attempt. Every consumed slot is
checked against the requested conversion: byte/int/fix supply four-byte integer
bits, big supplies eight-byte integer bits, real supplies exact binary64 bits,
String requires a reference slot with exact Tstring identity, and debug H accepts
a typed reference. Width stars also consume checked integer slots. Missing slots
raise exBounds; incompatible kinds raise exInval. All these checks remain active
when output fills. Extra unused slots retain ordinary printf semantics.

Eight-byte slots contain native-order values. Integer decoding narrows to UWORD
and copies its four-byte bit pattern, independently of host byte order; managed
references are converted from uintptr bits; real/big retain all eight bytes.
This adapter never constructs a host va_list or relies on legacy pointer strides.
The context is trusted constructed storage, immutable and pinned for the complete
synchronous attempt. Pattern/reference provenance and exact live Type identities
are established by the constructor's trusted schema callbacks. The formatter
cannot authenticate arbitrary C pointers passed as purported contexts.

The caller controls output retry/growth and result publication. The tests run the
actual shared parser and lib9 under native64argumentscall: a producer formats,
allocates an actual managed String and adopts it before collection; the resolver
publishes only on success. Sys_sprint/aprint/print/fprint still explicitly refuse
the native64 variadic path. Full startup remains blocked pending complete builtin
ABI and safe native64 registration/wiring. This private formatter is one part of
that work, not a complete replacement Sys interface.

## Reproduction and limits

```
python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/fresh/limbo
NATIVE64_CFLAGS='-O3 -DNDEBUG -g -fstrict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/fresh/limbo
```

Per emu/os and WORD32/WORD32-file combination, 1,728 formatter transactions cover
three collector phases, eight process states, nine format/type modes and eight
capacities from one to 128 bytes. Mixed signed/byte/fix/big/real values, star width,
Unicode characters, ASCII/Rune patterns and strings, H strings, embedded NUL,
repeat attempts, full buffers, wrong scalar/reference kinds and missing tail
arguments are tested. They retain the previous 456 construction/combined-call
transactions per combination and the frame-pin/Type-owner removal controls.
Another separately compiled control removes the typed conversion refusal and
must fail an actual result/error predicate. Real lib9 conversions, newstring/
newrunes, heap, Type, collector and protected-port bodies remain in the test;
bounded pool and deterministic native/process error text are fixture services.

Existing public packed-format, span, I/O and result-owner gates also run to check
the legacy reader after extraction. The complete runt.c translation unit compiles
with the native64 Sys header and formatter switch for NDK ARM64/x64. These are
compiler/hosted tests, not full emulator boot or physical device evidence. The
previous ASan failure during injected error unwinding remains open; this series
does not suppress or claim to resolve it. No Dis64 wire identifiers are assigned.

## Bounded growth and managed results

native64formatgrow constructs a Native64Formatted owner with an explicit initial
capacity and upper limit. It refuses invalid geometry and a nonempty output owner
before allocation. Each retry starts a fresh typed cursor. Capacity doubles only
when that cannot overflow or exceed the limit; the final step clamps to the exact
limit. The shared parser's UTF safety margin is retained conservatively. Reaching
the limit without proving complete output raises exNomem rather than publishing
possibly truncated output. Length counts bytes, including embedded NULs.

Mutable output ownership lives outside kprotect's jump frame. Every allocation
and parse failure releases acquired output, resets the owner and restores the
original VM diagnostic after free. Retirement detaches first and is repeatable.
The caller must keep arguments/pattern rooted and immutable throughout all retries.

native64formatstring and native64formatarray compose growth with actual c2string
and mem2array respectively. String results use normal VM UTF decoding and Rune
storage; byte arrays retain exact UTF bytes and NULs. The enclosing result guard
owns a value before fallible pinning. A second protected boundary retains native
output through conversion/pinning and frees it on all success/refusal/error paths.
These helpers are synchronous producers for an enclosing native64argumentscall
transaction; they do not publish a caller destination themselves or enable Sys.

The extension adds 384 growth/result transactions per port/model (1,536 total).
Sixteen scenarios across three collector phases/eight process states cover early,
intermediate and final growth allocation refusal; hard output limit; String and
byte-array heap/pin/resolver failures; late conversion mismatch; embedded NUL;
exact UTF-to-Rune conversion; and byte-preserving UTF arrays. Output exceeds the
initial capacity and exercises repeated parsing. A separate successful probe
checks the non-power-of-two 400-byte clamp, refusal to overwrite a live buffer,
invalid geometry without allocation and repeated retirement. Native frees
intentionally alter the error text; the original success/failure diagnostic must
survive. A separately compiled control removes final native-output retirement
and must fail allocation-balance predicates, alongside the existing kind/pin/Type
controls. Complete actual c2string and mem2array bodies are included in the gate.
The earlier ASan unwind finding remains open and is not suppressed here.
