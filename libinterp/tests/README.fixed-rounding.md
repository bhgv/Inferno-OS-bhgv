# Fixed coefficient rounding and compiler zero traps

This series extends existing decoded-operation leaf paths; it does not add new
opcode IDs or claim a complete native block compiler.

## Runtime changes

ARM64 sign-extends WORD32 frame coefficients before multiplication and divisor
checks. Previously a negative coefficient was zero-extended, producing wrong
results and bypassing the -1 fallback guard. Replaying the previous emitter
against the coefficient matrix reproduces a failure.

AMD64 and ARM64 now implement WN rounding for MULX1, DIVX1 and CVTXX1 when the
encoded scale requests a right shift of 1 through 63 bits. The modular rounding
mask is selected using the original operands, before multiplication can wrap.
DIVX1 uses the numerator's nonpositive sign condition. VN, unsupported scales,
and unsafe divisors retain the C path without speculative destination writes.
Both WORD32 and WORD64 layouts retain four-byte-aligned frame access support.
The generated paths fit the existing 64-unit buffers and make no helper calls.

## Compiler change

When fixed scaling always produces zero, division must still check its divisor.
The Limbo generator now emits integer 0/divisor instead of MOVW zero, preserving
exZdiv. Multiplication and conversion keep their zero-result optimization.

## Validation

- 324,000 WN C/native differential cases across WORD32 and WORD64, with ASan and
  UBSan, cover signs, wraparound, rejected flags/scales, aliases and alignment.
- 8,781,856 existing coefficient C/native cases pass across both WORD models.
- Unicorn emitter checks: 137,236 rounded coefficient cases, 30,316 coefficient
  cases and 29,170 Unicode string cases after shared ARM fallback refactoring.
- The compiler regression connects actual fixed type selection and lowering to
  actual WORD32 VM division/move handlers: 28,800 cases pass with sanitizers.
  The previous generator fails its zero-divisor assertion. This fixture is not
  a full parser-to-Dis or operating-system acceptance test.
- Both complete native AMD64 runtime matrices and ARM64 publication/failure/
  retry/freecode tests pass; the core suite passes all 56 host drivers.

Run the targeted tests from the repository root:

```
FIXED_COEFFICIENT_CASE=wn python3 libinterp/tests/fixed-coeff.py
python3 limbo/tests/fixed-zero-div.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=roundedcoefficient python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/jit-coverage.py
```

Physical ARM64/RV64 execution and full native 64-bit VM ABI acceptance remain
unverified. Coverage remains 109/173 opcode IDs with native paths for AMD64 and
ARM64; individual paths can be guarded or partial. RV64 coverage is unchanged.
