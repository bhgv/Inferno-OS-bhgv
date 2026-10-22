# Sys formatting regression tests

Run `python3 libinterp/tests/sys-print.py` from the repository root.

The fixture compiles production `xprint`, `bigxprint`, `Sys_sprint`, `Sys_aprint`
and their helpers with actual VM declarations, real lib9 formatting, and the
production emulator `kprotect`/`krethrow` bodies. It covers:

- Exact output prefixes and guards across capacities 0 through 64, with repeated
  literal/integer conversions after the output fills.
- ASCII and Rune format strings, trailing percent signs and incomplete fields.
- Dynamic widths, packed four-byte Dis integer slots on LP64 hosts, and mixed
  word/big arguments including the minimum signed 64-bit integer.
- Overlong specifications, repeated big modifiers, growth overflow, allocation
  failure, and errors raised after allocating a temporary output buffer.
- Stack and dynamically grown `sprint`/`aprint` results, injected VM conversion
  failures, unchanged old result owners, and exact temporary-buffer ownership.

Each WORD32/WORD64 model runs 158 cases with strict UBSan by default. For ASan:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-print.py
```

VM result constructors and destruction use bounded allocation/failure stubs;
they do not run GC. Pointer arguments and a full 64-bit builtin calling ABI are
not validated by this test. WORD32 retains native pointers. This is not an
Inferno boot, a compiler-generated varargs integration test, or JIT execution.
LeakSanitizer process inspection is disabled for this sandbox; ASan/UBSan remain
enabled. `SYS_PRINT_SOURCE` selects an alternate source, and `SYS_PRINT_MODELS`
selects `native`, `word32`, or both.
