# Real-to-fixed conversion bounds

Run from the repository root:

```sh
python3 libinterp/tests/real-fixed.py
CFLAGS='-O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer' python3 libinterp/tests/real-fixed.py
```

The fixture extracts production cvtfx and compiles WORD32 and WORD64 scalar
models. Each passes 30,792 checks under UBSan including float-cast-overflow and
ASan+UBSan. The independent oracle applies long-double truncation to the rounded
double product and compares against exact signed-word bounds. Tests include ties,
next-to-boundary values, both zero signs, subnormals, NaN/infinities, exceptional
products and 10,000 deterministic arbitrary-bit input/factor pairs. Destination
aliases each source and the coefficient, as well as a separate output slot.

The instruction validates its rounded product before the C integer cast. Invalid
values raise exInval while preserving all destination bytes. Valid conversions
retain the existing multiply/half-away rounding order and native word width.
Zero or negative factors are not rejected independently: representability of
the resulting product determines success. Ordinary real-to-int and real-to-big bounds are tested separately in
README.real-integers.md.

Baseline cvtfx reproduces NaN-to-integer float-cast-overflow. Existing native
JITs punt this opcode to the shared interpreter implementation. Their fallback
ABI, hardware floating-point environment and native signal translation are not
executed here. The test models scalar register storage and local setjmp errors,
not real VM stack frames or full Inferno boot. Active layouts/opcodes remain
unchanged. LeakSanitizer inspection is disabled for the sandbox; the fixture
has no dynamic heap allocations.
