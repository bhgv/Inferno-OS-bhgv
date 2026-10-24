# Native real arithmetic and Math result slots

## Five additional native opcode paths

AMD64 and ARM64 now emit IADDF, ISUBF, IMULF, IDIVF and ICVTXF. Both operands
are captured before the destination is written, including partially overlapping
and unaligned slots. CVTXF converts the signed active VM WORD to binary64 and
then performs a separate multiplication; it does not fuse or replace either
rounding step. Division uses host IEEE arithmetic, as the C interpreter does,
including floating division by zero rather than the integer exZdiv contract.

AMD64 uses baseline scalar SSE2 with volatile XMM0/XMM1. ARM64 uses volatile
D0/D1 and the existing AAPCS64 pointer argument. Neither path changes native
stack state, calls helpers, changes FP control registers, allocates, or changes
GC-visible ownership. Arithmetic can update sticky FP exception flags, just as
the corresponding C operation does. The current host floating environment is
inherited. NaN payload selection remains platform-defined.

These are decoded-operation leaves, not a native basic-block compiler. Complex
Math operations continue to use their optimized C implementations. RV64 JITs
are unchanged in this series.

## Sixty Math bindings with alignment-safe returns

Fifty-eight scalar-return bindings previously assigned through `*f->ret`.
A VM return pointer need not satisfy the native alignment of REAL, LONG or a
legacy WORD64. Each binding now computes a typed local result and copies its
bytes to the VM slot. This includes the four scalar vector queries; the vector
arithmetic itself remains in C.

The lgamma and modf tuple bindings also stage results at native alignment.
They use the actual generated return type's size, preserve existing padding,
and copy only after evaluating the result and validating conversion bounds.
No frame or tuple ABI layout is changed.

The previous source fails the new scalar-return fixture under UBSan with a
misaligned REAL store. The fixture extracts actual generated frame declarations
and production binding bodies. It compares aligned returns with all eight byte
offsets and returns overlapping inputs, checking exact bytes and FP flags in
four rounding modes. Host libm supplies fdlibm entry points in this binding
fixture; this does not independently validate fdlibm's numerical algorithms.
Existing numerical and bounds fixtures remain in the suite.

## Verification

- Complete native AMD64 matrices pass under ASan/UBSan for WORD32 and WORD64.
  The real arithmetic part covers 10,285,056 alignment/overlap/FP-state cases
  per WORD model, including special values and all four rounding modes.
- Unicorn/Capstone checks pass: 21,370 arithmetic instruction/ABI/alias/oracle
  checks across AMD64, ARM64 and the unchanged RV64 rejection paths.
- The new Math return fixture passes 23,616 cases per WORD model for 56
  non-vector bindings, including tuple padding and overlap.
- Vector return coverage adds every byte alignment and input aliases for dot,
  iamax, norm1 and norm2. The vector fixture passes 2,408 cases per WORD model.
- Math codec, special-value and exponent fixtures pass. Exponent fixture wiring
  now follows the staged assignment so it still calls the tested scalbn body.
- ARM64 production compiler publication/failure/retry/freecode checks pass for
  both WORD models. The core checkpoint passes 57 host drivers.

Physical ARM64 execution and a complete native 64-bit VM pointer/frame/GC ABI
remain separate acceptance work. ARM64 instruction emulation is not hardware
FP-environment acceptance.

```
python3 libinterp/tests/math-return-slots.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realarithmetic python3 libinterp/tests/leaf64-jit.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/leaf64-runtime.py
LEAF_WORD_BITS=32 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/leaf64-runtime.py
python3 libinterp/tests/jit-coverage.py
```

Of 173 active Dis commands, AMD64 and ARM64 each now have 114 native opcode
paths and 59 interpreter-only commands. This availability count includes guarded
partial paths. RV64+M remains 103/70, RV64+Zmmul 90/83, and RV64I 87/86.
