# Fixed-type scale validation

Run from the repository root:

```sh
python3 limbo/tests/fixed-scale.py
```

The fixture compiles production ckfix with minimal Type/Node/diagnostic models
and the host math library. Its 2,089 cases pass UBSan including float-cast-overflow,
and ASan+UBSan. It checks ordinary finite scales across binary exponents -900
through 900, state-count boundaries, the implicit full-width maximum, NaNs,
infinities, invalid signs, ratios outside the representation and normalization
underflow. Every rejection must emit one diagnostic and preserve the original
scale bit pattern.

Before computing the integer state count, ckfix requires a positive finite scale,
a finite nonnegative maximum and a rounded ratio below 2**31. This bounds the
following integer arithmetic and power-of-two search. Normalization uses an
unsigned wide power of two and publishes its scale only when it stays positive.
The baseline implementation triggers float-cast-overflow on an infinite ratio.

The zero maximum remains the existing internal sentinel for a full-width fixed
type. Ordinary accepted finite inputs retain their normalization. Bytecode and
JIT interfaces are unchanged. The fixture uses a host finite() implementation;
it does not test the complete parser/type checker, fixed arithmetic code
selection, runtime fixed arithmetic or the target fdlibm implementation.
ASan runs disable LeakSanitizer inspection because the sandbox hides /proc.
