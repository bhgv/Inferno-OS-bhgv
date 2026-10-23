# Directional functions and signed zero

Run from the repository root:

```sh
FDLIBM_CASE='atan2 nextafter' python3 libmath/tests/ieee-boundaries.py
python3 libmath/tests/ieee-boundaries.py
```

atan2 now checks the positive-unit high and low words using equality instead
of subtracting 0x3ff00000 from a signed high word. The old subtraction overflowed
for negative arguments, including -0.0 and -1.0. Quadrants, signed zeros,
nonfinite inputs, subnormal values and deterministic random pairs are checked
against host atan2; finite nonzero answers allow two ULPs of difference.

nextafter returns the destination when x==y. Numeric equality includes opposite
zero signs, so returning x previously produced the wrong sign for (-0,+0) and
(+0,-0). The fixture compares exact bits with host nextafter for equal values,
adjacent values, sign crossings, subnormal/normal boundaries and random pairs.
NaN payloads are not constrained.

FDLIBM_ATAN2_SOURCE and FDLIBM_NEXTAFTER_SOURCE select independent baselines.
The original atan2 fails UBSan, and the original nextafter fails the signed-zero
assertion. The complete shared driver performs 135,951 oracle cases across nine
functions, including all four rint rounding modes. Source-level limitations and
build options are documented in README.subnormal-bounds.md. This does not test
execution through a VM builtin frame or generated native JIT instructions.
