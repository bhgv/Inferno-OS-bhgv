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

Each WORD32/WORD64 model runs 1,014 cases with strict UBSan by default. For ASan:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-print.py
```

VM result constructors and destruction use bounded allocation/failure stubs;
they do not run GC. Native pointer/REAL/LONG reads at independent alignments
are covered; a complete compiler-generated 64-bit builtin varargs ABI is not
validated by this test. WORD32 retains native pointers. This is not an
Inferno boot, a compiler-generated varargs integration test, or JIT execution.
LeakSanitizer process inspection is disabled for this sandbox; ASan/UBSan remain
enabled. `SYS_PRINT_SOURCE` selects an alternate source, and `SYS_PRINT_MODELS`
selects `native`, `word32`, or both.

`SYS_PRINT_CASE=alignment`, `invalid` and `embedded` select the new isolated
ASan/UBSan families; `legacy` selects the original matrix. See
README.movpc-print.md for native PC materialization and embedded-NUL checks.


Latest output/argument spans, packed borrowed string views, native-width `%H`
and dynamic-result publication: [README.sys-print-spans.md](README.sys-print-spans.md).

[Public result snapshots](README.sys-print-public.md) check complete fixed frame
prefixes and return cells, packed frame access, original-cell publication across
allocation callbacks, shared old owners and error cleanup.

[Native output snapshots](README.sys-print-io.md) cover emu/os print, fprint
and werrstr frame/result preflight, callback-safe publication and native FD
width checks, with the actual formatter and controlled I/O boundaries.
