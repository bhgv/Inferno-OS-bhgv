# Independent signed division boundary oracle

`division-powers.py` extracts unchanged actual `divw`, `divl`, `modw` and `modl`
bodies from `xec.c`. A small native frame/error boundary invokes them with
Python arbitrary-precision expected values. This verifies arithmetic and byte
access, not instruction decoding, scheduling, whole-module loading or native JIT.

```sh
python3 libinterp/tests/division-powers.py
CFLAGS='-O1 -g -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' \
    python3 libinterp/tests/division-powers.py
```

`TMPDIR` may select an existing writable scratch directory. Generated vectors and
binaries stay there; no fixture is written into the owning source snapshot.
The default build uses O3/NDEBUG and strict UBSan. No assert is used as an oracle.

Both WORD32 and WORD64 builds retain big64 separately. Every positive and negative
power-of-two divisor, MIN and +/-3 is exercised against signed extrema, values
beside divisor boundaries, small negative/positive values and deterministic
full-width samples. Expected division truncates the mathematical magnitude
toward zero. Remainder is computed before the sole wrapping quotient case,
MIN/-1, so MIN%-1 is independently zero.

WORD32 runs9,422 vectors across its two scalar kinds; WORD64 runs12,558. Each
vector executes both quotient and remainder with separate, divisor-aliased and
dividend-aliased targets:131,880 successful owning handler calls overall. All
operands/targets are deliberately unaligned. Every byte outside the actual
result span must retain its sentinel or original operand byte.

Four handlers per model also receive a valid zero divisor with H dividend and
nil destination: zero-divide must win before those invalid operands are touched.
An independently compiled control changes the actual MIN/-1 quotient result to
zero. The active byte oracle must fail in both models; sanitizer termination
alone is not accepted as proof that the oracle works.

FlashVM contributes this gate alongside its own FIR0.72 known-bit analysis for
positive-power signed division. FIR successful-result facts do not remove eager
producer errors. This test confirms the current owning integer behavior; it does
not assign new Dis64 wire IDs, change admission or establish a new calling ABI.
