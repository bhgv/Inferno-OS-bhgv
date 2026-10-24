# Bessel order and recurrence boundaries

Run `python3 libinterp/tests/math-bessel.py` from the repository root. `CC` and
`CFLAGS` select the host compiler and flags; strict UBSan is enabled by default.
For ASan as well:

```
CFLAGS='-O2 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/math-bessel.py
```

Math.jn and Math.yn now validate the complete VM order before conversion to the
native signed 32-bit order API. Out-of-range wide WORD values raise exInval and
preserve the result. Saturating an order would change its parity and function
value; silently passing its low bits is also incorrect. This does not implement
a new 64-bit-order Bessel API or change the VM's current word declaration.

The production fdlibm recurrences retain the original int API while widening
order magnitudes, doubled indices and continued-fraction bounds to vlong.
INT_MIN can be negated safely, and n+n, 2*(n+k) and i+i no longer overflow int.
NaN classification tests the low representation word with unsigned negation,
so a low word of 0x80000000 is valid data rather than a signed-negation overflow.
The Y recurrence applies negative-order parity before its zero-pole and positive
infinity returns: odd negative orders produce positive infinity at zero and
negative zero at positive infinity, consistent with Y(-n,x)=(-1)^n Y(n,x).

The fixture compiles the complete production e_jn.c bodies and the real Math
bridges, with builtin frames extracted from runt.h. Host libm supplies j0/j1/y0/y1
base cases and independent jn/yn results for ordinary arguments. Comparisons
check numeric classes, zero signs and relative rounding differences. Pole and
infinity signs are checked independently from the integer-order identity because
the host yn implementation also has early-return sign differences at these
boundaries. No system Bessel implementation is used as the extreme-order oracle.

Each WORD model exercises orders -64 through 64 with arbitrary low-word special
values and a grid of positive arguments, plus 4,096 deterministic order/argument
pairs. VM outputs also alias their real argument slots. Analytic fast paths cover
INT_MIN, INT_MIN+1 and INT_MAX with zero, tiny arguments, infinities, NaNs and
large-argument asymptotes. Wide VM rejection checks preserve the old result.
There are 54,102 checks in WORD64 and 54,084 in WORD32, totaling 108,186.
The full fixture passes UBSan and ASan+UBSan with stack instrumentation enabled.

Five huge backward-recurrence cases run production continued-fraction setup but
stop at the logarithm adapter before potentially billions of recurrence steps.
These probes verify defined index setup; they do not establish a full numeric
answer, convergence or bounded latency for arbitrary extreme finite orders.
NaN payload preservation and floating-point exception flags are not asserted.

`BESSEL_CASE=ordinary`, `extreme`, `indices` or `wide` selects a subset.
`BESSEL_NATIVE_SOURCE` and `BESSEL_VM_SOURCE` substitute alternate source files.
Parent substitutions independently reproduce signed low-word negation, INT_MIN
order negation, doubled-index overflow and missing wide-order rejection. A
variant retaining the old Y early returns while using safe arithmetic separately
fails the parity oracle. These are actual production-body failures, not modeled
substitutes for the corrected arithmetic.

The shared `slice-tokenize-suite.py --core` runner includes this test. Its 26
VM/compiler drivers pass, along with adjacent Math special-value and exponent
fixtures and ARM/386-header syntax checks. Compiler and JIT backend source is
unchanged in this series. WORD32 changes scalar declarations while retaining
native host pointers; no complete ABI32, VM boot, GC or native target JIT run is
claimed. Tk, security/cryptography, MIPS, SPARC and PowerPC remain untouched.
