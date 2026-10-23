# Numeric folding result types

Run `python3 limbo/tests/fold-result-types.py` from the repository root.
`CC` and `CFLAGS` select the host compiler and flags; UBSan is enabled by default.
`NODES_SOURCE` selects an alternate revision of `limbo/nodes.c`.

The fixture extracts production recursive folding, identity, effect and power
helpers and reuses the bounded AST/type adapters in `fold-identities.c`.
It checks 25,369 cases over byte, int and big arithmetic, comparisons, bitwise
operations, shifts and integer powers. Constants, ordinary variables and
observable calls are tested against an independent modular integer evaluator.
Assertions check the declared result type as well as the value and call count.
Inputs include signed extrema and big values outside the int range. Invalid
arithmetic is covered separately by the exception and short-circuit suites.

A pure `big` variable raised to zero must yield a `big` constant, even though
the exponent has type `int`. Folding into the original expression preserves
its declared type and source position. A composed `(x ** 0) << 40` case checks
that all high result bits survive. An effectful base must still be evaluated.
The parent source fails the result-type assertion.

The minimal effect/power fixture now includes the existing AST declaration
field, allowing it to exercise constant-node cleanup as production does.
This suite does not run the complete compiler, VM or any native JIT backend.
