# Integral rounding and carry boundaries

Run from the repository root:

```sh
FDLIBM_CASE='ceil floor rint' python3 libmath/tests/ieee-boundaries.py
```

The shared driver compiles the production functions under strict UBSan.
ceil/floor are compared bit-for-bit with Python's integer rounding operations;
zero results retain the input sign. Cases cover word carries, 2^21 boundaries,
quarter/half/three-quarter fractions, neighbors of halfway values, subnormals,
nonfinite values and deterministic arbitrary binary64 representations.

ceil/floor now form their low-word increment using 1U instead of signed 1.
This avoids both a shift into bit 31 and signed addition overflow before the
result reaches the unsigned carry variable. rint restores its sign using an
unsigned shift. Its fractional sticky-bit mask is now 0x40000, so it remains
nonzero at exponent 18. The old mask lost that bit: -262144.75 became -262144
instead of -262145 in round-to-nearest mode. Fixing only the sign shift still
fails the numerical assertion, independently demonstrating the rounding defect.

rint is also tested under FE_TONEAREST, FE_DOWNWARD, FE_UPWARD and FE_TOWARDZERO.
A native helper selects the host's actual fenv constants. Python's independent
round/floor/ceil/trunc results are prepared before switching modes, and the
worker restores the original default mode after every group and on exceptions.
The build uses -frounding-math and -fno-builtin. NaN payloads and floating-point
exception flags are not compared.

FDLIBM_CEIL_SOURCE, FDLIBM_FLOOR_SOURCE and FDLIBM_RINT_SOURCE select baseline
sources individually; all three original files fail under UBSan. See
README.subnormal-bounds.md for the shared driver's native-host limitations.
