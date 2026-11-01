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

## Optimized predicates (series 0055)

The fixture now uses explicit `if` failures throughout. Earlier `assert` predicates
were disabled by `-DNDEBUG`, so an optimized successful exit from that old fixture
did not establish its semantic comparisons. Each model now also compiles a
production-result mutation (ASCII encoding reports2 instead of1); the independent
alignment/byte-count oracle must reject that executable with a semantic diagnostic.
Compiler/link failure alone cannot satisfy this control.

`NATIVE64_RUNNER` selects an explicit executor for both normal and mutated C
executables. The clean native64 port driver runs the owning test on x64, ARM64
and RV64, using its selected target compiler/flags. Each model retains1,115,061
cases. Cross targets are executed under QEMU; this is not physical hardware or
whole-VM builtin/managed-call admission.

The O3/NDEBUG strict UBSan host run and target O3 runs pass with active predicates.
ASan+UBSan alignment-only cases and their semantic controls pass. The complete
ASan error-unwind run still reproduces the stack-redzone report after ASan warns
that `__asan_handle_no_return` has no known stack top. That report remains open;
the rewritten assertions are not a suppression or a claim of ASan closure.
