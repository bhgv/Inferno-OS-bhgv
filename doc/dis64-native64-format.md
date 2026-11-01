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
