# Sys UTF builtin regression tests

Run `python3 libinterp/tests/sys-utf.py` from the repository root.
The driver compiles the production `Sys_char2byte`, `Sys_byte2char`,
`Sys_utfbytes`, and their validators with the real generated builtin frames,
array/heap declarations, and `lib9/rune.c`.

The independent encoder covers every value from zero through U+110000,
including surrogates, U+10FFFF, and the first out-of-range value. Tests check
complete bytes, decode tuples, every incomplete prefix, output capacities,
nonzero offsets, malformed encodings, and surrounding guard bytes. Invalid
full-word offsets, rune values, array lengths, metadata, and data pointers must
leave output values and bytes intact. An empty prefix accepts a nil data span.

Both WORD64 and WORD32 scalar declarations run with native host pointers.
Each model checks 1,114,154 scalar/bounds cases plus capacity/prefix assertions.
Defaults enable strict UBSan. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-utf.py
```

VM errors use a bounded longjmp fixture. This does not boot Inferno, execute
the builtin through a JIT, reproduce a complete 32-bit pointer ABI, or run GC.
LeakSanitizer process inspection is disabled for the sandbox; AddressSanitizer
and UndefinedBehaviorSanitizer remain enabled. `SYS_UTF_SOURCE` can select an
alternate production file for regression comparisons.

[Complete UTF span checks](README.sys-utf-spans.md) add packed generated frames,
Array/Type inputs, complete WORD/tuple results, overlapping publication and
preflight failures before payload reads or writes.
