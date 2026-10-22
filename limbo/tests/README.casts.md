# Constant cast folding regressions

Run from the repository root:

```sh
python3 limbo/tests/fold-casts.py
```

The fixture extracts production foldcast, foldcasti, fix2real and the integer
conversion helpers. It checks 73,860 source/destination, width, rounding and
unmodified-AST cases. UBSan and float-cast-overflow instrumentation are enabled
with immediate failure by default. AST allocation, string parsing and fixed-point
scale lookup are bounded dependency models; production numeric parsing has
separate tests in numeric-lexer.py. Formatted-string and fixed-point destinations
are outside this fixture.

Integer sources cover int, byte and big, with every integer destination plus real,
using signed boundaries and deterministic full-width values. Widened arithmetic
provides the signed-32 oracle. The compiler's existing Dis int32 model is retained
independently of native host long size. Signed extension no longer shifts a
positive signed value into the sign bit of Long.

String-to-integer results now pass through the same destination normalization as
integer casts. Real and fixed-point sources also normalize byte results instead
of attaching byte type to a value outside 0..255. Real conversion retains the
existing half-away-from-zero expression used by the interpreter; fixed-point
sources retain their existing truncation behavior before narrowing.

Real-to-integer folding first verifies that the rounded intermediate fits Long.
Int/byte paths additionally require a representable Dis int32 intermediate.
Comparisons reject NaNs and infinities before any native integer cast. Unsafe
cases return the original cast expression without modifying the source constant,
including its type, source location and value. Boundary tests use long-double
truncation as the range oracle and include nextafter neighbors around signed-64
limits. Fixed-point tests cover scale 1/4 and boundary scales 1 and 2.

Deferring an unsafe fold does not define new runtime semantics for out-of-range
real-to-integer conversions. Such conversions remain ordinary runtime expressions;
contexts requiring a compile-time constant may consequently reject them instead
of depending on an undefined host C conversion. No VM opcode, builtin signature
or active ABI layout changes are made by this patch.

Baseline probes reproduce signed-shift undefined behavior, missing string/real/
fixed-point byte narrowing, and nonfinite native conversion. This is a focused
host fixture, not a complete compiler build or target JIT execution test.

A supplementary AddressSanitizer+UBSan+float-cast-overflow run also passes:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer' python3 limbo/tests/fold-casts.py
```

Stack instrumentation remains enabled in this fixture, which does not unwind VM
errors. LeakSanitizer is explicitly disabled because process inspection is
unavailable in the sandbox; the fixture uses no heap allocation.
