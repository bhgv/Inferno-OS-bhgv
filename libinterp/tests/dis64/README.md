# Dis64 prototype fixtures

These fixtures describe the jointly reviewed map8 and typed-table direction.
No magic, ABI, section or opcode registry IDs are assigned. The active compiler,
loader, interpreter and JIT do not include these prototype headers yet.

Run from the repository root:

```sh
python3 libinterp/tests/dis-layout.py
python3 libinterp/tests/dis-control.py
CFLAGS='-O1 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis-layout.py
CFLAGS='-O3 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis-control.py
```

`dis-layout.c` uses always-on `if` checks. It covers ABI32/map4 and ABI64/map8,
int32/ref width separation, explicit frame slots, nested records, int-only
stride4, bounded array extents, atomic failures, and 2,304 single-bit map cases.
Full maps reject missing/excess bytes and marked incomplete references.
This prototype's full ABI32 maps do not redefine existing classic Dis files.

`control-golden.json` contains hand-written BE payload bytes and expected valid
or invalid classifications. They include signed MIN/MAX inclusive ranges,
string ranges whose ID order differs from content order, embedded U+0000,
Latin-1/Rune/supplementary values and goto order. The string fixtures use a
resolved pool; they do not choose the future pool encoding or a nil sentinel.

`dis-control.py` compiles the real candidate C helper and compares 6,112
complete/truncated/mutated/PC-bound cases to an independent Python oracle.
It also verifies that rejection leaves the output view unchanged. The helper
checks payloads without constructing VM heap objects or native-PC caches.

These are helper tests on the current compiler host, not executable new-ABI
modules, host32 cross builds, physical ARM64 runs or GC/ref-result acceptance.
For design/open decisions see `doc/dis64-section-matrix.md` and the two journals.
