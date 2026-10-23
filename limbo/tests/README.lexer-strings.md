# Compiler string and identifier regressions

Run `python3 limbo/tests/lexer-strings.py` from the repository root.
The harness extracts production functions from `limbo/lex.c`; it does not
substitute implementations of interning, concatenation, or buffer growth.
Both signed-char and unsigned-char builds run with UBSan by default.

Covered regressions:

- High-bit bytes in identifiers and interned string data must not cause
  signed left shifts; duplicate strings, including embedded NULs, intern once.
- Empty strings release their owned buffer before interning the empty symbol,
  without inspecting an uninitialized first byte.
- Empty and unterminated literals work with poisoned allocation contents.
- A 100,000-byte literal requires fewer than 20 reallocations. Capacity is
  measured in bytes, rather than multiples of the native pointer size.
- Identifier lengths at and above the 256-rune limit distinguish accepted
  input from a diagnostic, while preserving the following delimiter.
- Concatenation rejects lengths that leave no room for a signed-int-sized
  allocation and its terminator before copying any source bytes.
- Suppressed fatal diagnostics still pass an initialized, nonempty failure
  status to `exits`.
- Diagnostic copying accepts an empty output span without modifying storage
  and terminates a one-byte output span.

`LEX_STRINGS_SOURCE` can select an earlier `lex.c` to reproduce failures.
`CC` and `CFLAGS` override the compiler and default sanitizer flags.
AddressSanitizer can be enabled with `CFLAGS`; LeakSanitizer is disabled in
this sandbox because it cannot enumerate threads. Intern tables retain
symbols for the lifetime of the compiler, as in production.

The fixture uses ASCII input for rune delivery; it exercises all 255 non-NUL
byte values directly through production identifier and string interning.
It checks the concatenation size boundary with synthetic lengths, without
requiring multi-gigabyte allocations. Full compiler builds and execution of
native JIT code are outside this harness.
