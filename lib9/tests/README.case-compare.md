# Native ASCII case comparison

Run `python3 lib9/tests/case-compare.py` from the repository root.

The fixture compiles the complete production cistrcmp/cistrncmp translation
units from both lib9 and libkern, independently with signed and unsigned char.
Each of the four configurations passes 364,710 scenarios under strict UBSan.
The oracle uses an independent ASCII lookup table and compares result signs,
which are the comparison API's ordering contract.

Every pair of leading bytes is checked with matching mixed-case suffixes,
reversed operands, embedded terminators and limits 0, 1, 2, 3 and INT_MAX.
All prefix-length pairs 0 through 32 are checked at limits 0 through 33.
Nonpositive limits including INT_MIN accept null pointers without reading them.

Both implementations compare unsigned bytes, fold ASCII uppercase on both
sides and stop on a mismatch or NUL. Bounded comparison tests the count before
accessing either string and decrements only a positive count. This removes
signed-char-dependent ordering at a terminator and INT_MIN decrement overflow.
No locale folding, Unicode case mapping, VM layout or opcode changes are made.

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 lib9/tests/case-compare.py
```

`CASE_LIB9_CISTRCMP`, `CASE_LIB9_CISTRNCMP`, `CASE_LIBKERN_CISTRCMP` and
`CASE_LIBKERN_CISTRNCMP` select alternate source files. `CASE_COMPARE_CASE`
selects `pairs`, `prefixes`, or `bounds`. Independent substitutions of each old
comparison body fail the byte-order checks; both old bounded implementations
also trigger UBSan on INT_MIN with `bounds`.

These are native helper tests, not full Limbo compilation, Inferno boot or
execution of generated JIT code. They use host headers selected through the
existing Linux/arm lib9 compatibility header, not target ABI emulation.
