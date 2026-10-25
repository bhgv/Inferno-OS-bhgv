# Native real powers, signed unit scales and Sys return slots

## Native EXPF

AMD64 and ARM64 now emit IEXPF. The leaf follows the interpreter's repeated
squaring order exactly: take the reciprocal for a negative exponent, multiply
the accumulator for each set bit, shift the unsigned exponent, and square only
when another iteration remains. This is not a substitution with host libm pow,
which can have different rounding behavior.

The minimum signed WORD exponent is handled through modular negation and logical
shifts, without signed overflow. WORD32 exponents are sign-extended before the
native calculation. Exponent zero returns exactly 1.0 without evaluating an FP
operation on the base, including signaling NaN bases. Signed zero, infinities,
NaNs, underflow, overflow and all host rounding modes retain the C operation
sequence. NaN payload choice remains host-defined.

Both decoded operands are captured before writing an unaligned or overlapping
binary64 destination. The leaves use caller-saved integer/FP registers, no native
stack frame, no allocation and no helper calls. Complex library operations
continue to use their shared optimized C implementations.

## CVTFX negative unit scale

The existing guarded CVTFX leaf now accepts both exact +1.0 and -1.0 scales.
It executes the floating multiplication and selects the half-unit rounding
correction from the resulting bits, including zero handling. The integer guards
still reject unsupported scales, non-finite inputs and large magnitudes before
any FP side effects. The existing conservative WORD32/WORD64 bounds remain.
This extends an existing opcode path; only IEXPF increases opcode availability.

## Twelve Sys binding fixes

Sys.read, readn, pread, write, pwrite and stream in both emu and os previously
stored WORD results through a potentially unaligned typed pointer. They now
stage a native WORD and copy it into the VM result slot. Short-transfer and error
returns, native counts/offsets, scheduler release/acquire order, stream ownership
and modular byte-count semantics are preserved.

The return-slot fixtures now exercise all eight byte offsets and verify neighbor
bytes. Replaying the previous sources reproduces UBSan misaligned WORD stores
in both the I/O and stream families. No Tk, security/crypto code or excluded
legacy JIT was modified.

## Verification

- Native AMD64 EXPF/C differential: 983,040 cases per WORD model, 1,966,080 total,
  with ASan/UBSan. It checks complete slot/REG preservation, all four rounding
  modes, preexisting sticky flags, special bases, minimum/maximum exponents,
  every operand byte alignment and six destination/partial-alias arrangements.
- Native real-to-integer regression, including both signed unit scales:
  5,505,024 cases per WORD model, 11,010,048 total, with ASan/UBSan.
- Unicorn/Capstone: 24,154 real-power and 43,546 conversion instruction/ABI/
  alias/oracle checks. ARM64 tests include FPCR preservation and FPSR flags in
  all four rounding modes. Accepted AMD64 exception flags are verified natively:
  Unicorn x86 does not model all precision flags.
- Sys I/O: 720 cases per platform/WORD configuration, 2,880 total, with
  ASan/UBSan; stream: 92,768 per configuration, 371,072 total. These include
  fragmentation, failures, bounds, cleanup and return-slot guards.
- Both full AMD64 runtime matrices pass with ASan/UBSan. Dispatch tests cover
  native and interpreter minimum-exponent power paths. ARM64 compiler RX
  publication/failure/retry/freecode tests pass in both WORD models; the RV64
  publication fixture passes with IEXPF remaining on the interpreter path.

The core checkpoint contains 60 host drivers. Physical ARM64 execution and the
complete native 64-bit VM pointer/frame/GC ABI remain separate acceptance work;
instruction emulation does not replace hardware validation.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/real-power-native.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realpower python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realinteger python3 libinterp/tests/leaf64-jit.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-io.py
python3 libinterp/tests/sys-stream.py
python3 libinterp/tests/jit-coverage.py
```

Of 173 active Dis instructions, AMD64 and ARM64 each now have 118 native opcode
paths and 55 interpreter-only instructions. Counts include guarded partial paths.
RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I 87/86.
