# Recursive constant-folding identities

Run from the repository root:

```sh
python3 limbo/tests/fold-identities.py
```

The fixture extracts production efold, folds, foldvc, foldc, foldr, sametree,
hasside and power helpers. Bounded AST allocation, operator metadata and unrelated
language dependencies are modelled. Tests execute the real recursive folding
pipeline and compare it with an independent expression evaluator.

The 3,043 cases include:

* Same-tree real comparisons and subtraction with NaNs, infinities, both zero
  signs, ordinary values and extreme magnitudes. These identities remain intact
  because x == x and x - x are not universally true/zero under IEEE semantics.
* x && x and x || x with nonnormalized integers and already-normalized
  comparison operands. Results must remain boolean. Pure normalized operands may
  still collapse; effectful call operands preserve their evaluation counts.
* Exact integer comparison/subtraction identities, ensuring the real guard does
  not disable valid int/big optimizations.
* Left-associated real addition/multiplication where regrouping constants changes
  rounding, overflow, underflow or NaN behavior. The existing constant-combining
  rewrite is limited to int, byte and big. Fixed-point ASTs are also kept out of
  that integer rewrite; exact integer combination remains enabled.

Baseline probes reproduce incorrect IEEE identities, loss of boolean
normalization, and changed real reassociation results. Tests default to UBSan
with immediate failure. A supplementary AddressSanitizer run also passes:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 limbo/tests/fold-identities.py
```

Stack instrumentation remains enabled. LeakSanitizer is explicitly disabled
because sandbox process inspection is unavailable; the fixture uses static
storage. The tests do not promise stable NaN payloads or model floating-point
exception flags. String/aggregate/cast AST paths are outside this fixture and
have separate regression coverage where available. No opcode or active ABI
layout changes are made. A full compiler/Inferno build and target JIT execution
remain outside this host fixture.
