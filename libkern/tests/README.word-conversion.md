# Fixed-width kernel integer conversions

Run `python3 libkern/tests/word-conversion.py` from the repository root.

The fixture compiles the production strtol, strtoul, atol and atoi bodies,
renaming only their public symbols and limit macros to avoid libc collisions.
Each of two accumulator models (32-bit unsigned int and native unsigned long)
runs 40,148 vectors. Python arbitrary-precision integers provide expectations.
The tests preserve libkern's fixed 32-bit result semantics on wider hosts.

Coverage includes bases 2 through 36, signed limits, unsigned limits and their
40 neighboring values on either side, large magnitudes through 128 bits,
4,096-digit overflow tokens, signs, whitespace, invalid bases, hexadecimal and
octal prefixes, exact end pointers and null end pointers. atol/atoi preserve
base-zero parsing, whitespace after the sign and 32-bit modular results.
The signed converter retains its historical no-digit hexadecimal-prefix rule.
Each of the three old source files independently fails this fixture.

Strict UBSan is the default. ASan can be enabled with
`CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'`.
LeakSanitizer process inspection is disabled; other instrumentation remains on.
`KERNEL_STRTOL_SOURCE`, `KERNEL_STRTOUL_SOURCE` and `KERNEL_ATOL_SOURCE` select
individual alternate bodies for before/after comparisons. This is a scalar
arithmetic test, not a complete 32-bit ABI build, native kernel boot or JIT run.
