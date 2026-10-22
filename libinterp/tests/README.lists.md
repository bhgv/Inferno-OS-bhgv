# Interpreter list construction regressions

Run `python3 libinterp/tests/lists.py` from the repository root.

The fixture compiles actual `cons`, seven constructor instructions, reference-map
helpers and emulator runtime protection with real Heap/List/Type/REG declarations.
A bounded heap supplies exact payload sizes, 32 guard bytes and injected failures.
Destruction checks mapped heads, tails, type references and complete heap balance.

Each WORD64/WORD32 model runs 1,053 cases covering scalar values, pointer width,
raw payloads of 0 through 257 bytes, shared tails, allocation failures, nil zero
spans and invalid full-word sizes. Aliased pointer/raw/mapped constructors must
capture their original source before publishing the resulting list. Mapped
payloads check both head and tail owners; rejected calls preserve the old list.
Each of the old consw/consp/consm/consmp bodies independently fails the fixture.

Defaults enable strict UBSan. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/lists.py
```

`LIST_SOURCE` selects alternate instruction bodies. WORD32 retains host pointers
and is not a full Dis ABI32 test. Scalar operand aliasing beyond typed pointer/raw
payloads is not covered. The fixture does not run GC, compiler-generated list
programs, JIT calls or a complete Inferno boot. LeakSanitizer process inspection
is disabled in the sandbox; ASan/UBSan remain enabled.
