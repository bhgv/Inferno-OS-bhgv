# Decimal powers and extreme exponents

Run `python3 lib9/tests/pow10.py` from the repository root.

Both production lib9 and libkern implementations are compiled under UBSan.
Each checks 2,206 cases against independently rounded Python Decimal values:
every exponent from -1100 through 1100 and signed-int extremes. Nonzero finite
answers allow four ULPs for the multiplication-based algorithm; zero and
infinity must match exactly. POW10_SOURCE selects an alternative implementation.

Negative exponents are now evaluated directly instead of inverting an already
overflowed positive power. This retains binary64 subnormals for -309 through
-323. The algorithm never negates INT_MIN and computes each half power once,
limiting extreme-exponent work to logarithmic recursion. The original lookup
table and its reciprocal results are preserved within the table range.

These host binary64 tests do not establish target FPU exception flags or
rounding-mode behavior. See libinterp/tests/math-exponents.py for native binary
scaling and the VM's separate integer-exponent bridges.
