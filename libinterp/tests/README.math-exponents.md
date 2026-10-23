# Native and VM math exponent bounds

Run `python3 libinterp/tests/math-exponents.py` from the repository root.

The driver compiles production Math_pow10, Math_scalbn, their exponent bridge,
libmath/ipow10 and fdlibm/scalbn under strict UBSan. WORD32 and WORD64 scalar
fixtures perform 77,409 and 78,681 checks. The native scaling oracle is the host
math.ldexp, separate from the renamed production fdlibm function. Values include
both zero signs, infinities, NaN, minimum subnormals, large finite values and
200 deterministic floating-point samples. Exponents include the signed-int
limits, fdlibm guard boundaries, normal/subnormal transitions, and 64-bit values
whose low 32 bits formerly reversed overflow/underflow or became zero/one.

VM exponents saturate to the native int range before calling the native math
functions. For binary64, an exponent outside that range is already far beyond
any finite nonzero scaling result. fdlibm handles zero and nonfinite input
first, then extreme exponents before the signed addition k+n. This avoids the
old signed-overflow fallback, which inspected the result after C undefined
behavior had already occurred.

SCALBN_SOURCE and MATH_EXPONENT_SOURCE allow independent baseline comparisons.
The fixture uses native pointers with 32/64-bit scalar words, simplified frames,
and little-endian fdlibm word access under -fno-strict-aliasing. It does not
establish a complete VM ABI, execute JIT instructions, or test target hardware
rounding modes and floating-point status flags. No Dis format or JIT changes
are required.
