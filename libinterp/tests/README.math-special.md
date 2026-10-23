# Scalar Math special values

Run from the repository root:

```sh
python3 libinterp/tests/math-special.py
CFLAGS='-O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all' python3 libinterp/tests/math-special.py
```

The fixture executes the production fdim/fmin/fmax functions, fdlibm modf and
their Math builtins. Generated builtin frame declarations, including the tuple's
explicit padding, come directly from runt.h. Each WORD32/WORD64 scalar model
runs 156,006 checks. Host libm supplies independent numeric results; signed-zero
selection for fmin/fmax is checked explicitly because host implementations can
choose different zero signs.

Coverage includes both argument orders, infinities, quiet NaNs, both zero signs,
normal/subnormal extremes, neighboring integer boundaries and 10,000 deterministic
arbitrary-bit pairs. Random NaNs are quieted to avoid depending on host signaling
NaN policy. Native modf must preserve a NaN in both components. Math.modf must
truncate toward zero, preserve fractional zero signs and reject an integral
component outside the active VM word's range. Invalid conversions retain all
destination tuple bytes. Successful calls preserve tuple padding. Result slots
also alias the builtin input fields to check read-before-write behavior.

Select a subset with MATH_SPECIAL_CASE=fdim, fmin, fmax, native-modf or modf.
MATH_SCALAR_SOURCE, MATH_MODF_SOURCE and MATH_SPECIAL_SOURCE substitute older
native scalar, native modf and interpreter implementations, respectively.
Each previous implementation fails its matching regression subset.

The default run enables UBSan and float-cast-overflow. The supplementary ASan
run disables LeakSanitizer process inspection; the fixture uses static storage
and no native heap allocations. fdlibm word access follows host byte order and
uses -fno-strict-aliasing, as required by its existing representation macros.
These are scalar host tests, not a full VM boot, native 32-bit ABI build or JIT
execution. Floating-point exception flags and NaN payload stability are not
asserted. No VM layout, instruction encoding or JIT backend is changed.
