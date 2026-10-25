# Rounded real conversions and geometry return slots

## Three guarded native conversion paths

AMD64 and ARM64 now emit ICVTFW, ICVTFL and ICVTFX. The implementation performs
the C operation's signed half-unit addition followed by truncating conversion.
It therefore retains rounding-mode and sticky-exception effects, including the
inexact flag from converting a half-unit result. A pure integer bit extraction
would not preserve that contract.

Integer representation guards run before any floating operation. They accept
finite inputs with magnitude below 2^30 for WORD32 outputs and below 2^62 for
64-bit outputs. These conservative bounds leave room for rounding in all four
host rounding modes. Larger inputs, infinities and NaNs return to C without
changing FP state or destination memory. Negative zero selects +0.5, matching
the C less-than comparison. C retains range-error and exceptional-input handling.

ICVTFX currently accepts only the exact +1.0 scale. It still executes the separate
floating multiplication before rounding, preserving the C operation sequence.
Other scales retain the C path. Both operands are captured before any destination
write; partial overlap and byte-offset VM slots are supported. The generated
leaves use caller-saved registers, no helper calls and no native stack frames.
RV64 support is unchanged.

These are partial decoded-operation leaves, not complete native basic blocks.
Complex operations continue to use their optimized C implementations.

## Nineteen geometry binding fixes

All Point/Rect entry points in geom.c now stage results at native alignment and
copy them into the VM return slot only after computing the complete result.
This removes unaligned typed stores and prevents output overlapping an argument
from changing coordinates that the function still needs to read. Rect_combine
previously overwrote its second input before comparing it; an aligned alias
regression reproduces this independently of alignment sanitizers.

Rect_clip stages its generated tuple representation and preserves existing ABI
padding. Point_div still raises exZdiv before publishing either coordinate.
Arithmetic and predicate formulas, coordinate widths and generated frame layouts
are unchanged. No Tk, crypto/security code or excluded legacy JIT was modified.

## Validation

- Native AMD64 conversion tests: 5,505,024 cases per WORD model (11,010,048 total)
  pass with ASan/UBSan. These compare actual interpreter bodies and JIT results,
  complete slot memory, REG state, all four rounding modes, preexisting FP flags,
  rejected scales and non-finite inputs, unaligned slots and partial aliases.
- Unicorn/Capstone: 42,004 instruction/ABI/alias/oracle checks execute both emitters against independent numeric
  oracles and a host C rounding/exception oracle. ARM64 checks FPCR preservation
  and FPSR flags in all four modes. Rejected paths preserve FP status on both
  architectures. Unicorn x86 omits conversion precision flags, so accepted AMD64
  exception flags are verified by native execution rather than emulation.
- Geometry return tests use the real generated frame declarations and all 19
  production functions: 27,136 alignment/partial-alias/exception cases pass per
  WORD model. The previous source fails UBSan on an unaligned Point return and
  fails the aligned Rect_combine alias comparison.
- The existing arbitrary-precision geometry arithmetic matrix passes for both
  WORD models. The core checkpoint passes all 59 host drivers.
- Full AMD64 runtime matrices pass with ASan/UBSan for both WORD models. Compiler
  publication tests now include all eight recent real arithmetic/conversion
  leaves and validate fallback-only modules after clearing the entire test
  instruction array. ARM64 publication/failure/retry/freecode tests pass for both
  WORD models; the RV64 publication fixture also passes.

ARM64 physical hardware and the complete native 64-bit VM pointer/frame/GC ABI
still require separate acceptance. Instruction emulation does not replace that.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/real-to-integer-native.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realinteger python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/geometry-return-slots.py
python3 libinterp/tests/geometry-arithmetic.py
python3 libinterp/tests/jit-coverage.py
```

Of 173 active Dis instructions, AMD64 and ARM64 each now have 117 native opcode
paths and 56 interpreter-only instructions. Counts include guarded partial paths.
RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I 87/86.
