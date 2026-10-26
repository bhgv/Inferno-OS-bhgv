# Fixed-point opcode selection

Run from the repository root:

```sh
python3 limbo/tests/fixed-ops.py
CFLAGS='-O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 limbo/tests/fixed-ops.py
```

The fixture extracts production fixed-ratio, normalization and opcode-selection
functions, supplies minimal Type/diagnostic models and links the actual WORD32
VM fixed-operation bodies. Its 25,029 checks pass UBSan including
float-cast-overflow and ASan+UBSan. LeakSanitizer inspection is disabled because
the sandbox hides /proc; the fixture has no heap allocations.

Exact powers of two across exponents -60 through 60 select direct shifts.
Neighboring representable doubles must not be classified as exact powers just
because their rounded coefficient matches one. Non-power ratios span six
mantissas and exponents -100 through 100. Returned opcodes, positive coefficients
and decoded shift limits are checked. A selected 1.5 fixed conversion is executed
for every input -10000 through 10000 against an independent exact truncation
oracle, using production runtime instructions rather than a simulated executor.

Scale products combine frexp mantissas and exponents before ldexp, retaining
representable final ratios when an intermediate product would overflow or
underflow. Explicit examples cancel very large/small finite scales to 2**25.
Invalid scale signs, NaN/infinity, unrepresentable ratios and unsupported shifts
produce source diagnostics before a dangerous instruction is emitted. Parameters
are initialized before selection. The existing zero-result IMOVW sentinel is
preserved through fixed-to-fixed conversion instead of being arithmetically
translated into an unrelated opcode.

Coefficient error estimates use real powers of two rather than C shifts with
negative or oversized exponents. Packing signed exponents uses multiplication
instead of shifting a negative int. Private normalization uses frexp rather than
unbounded magnitude loops; public selection validates its input ratio first.
Baseline probes independently reproduce near-power misclassification, negative
shift UBSan and the incorrect zero-conversion opcode.

This fixture models compiler helper behavior and exercises one compiler/runtime
conversion path. It does not validate every fixed rounding case end to end,
complete parsing/code emission, target libmath implementations, JIT fallback ABI
or full Inferno boot. VM layouts and the instruction set are unchanged.

## Frame dependency refresh

The selection fixture now extracts the production `fixedword` helper and writes
coefficients into an explicit WORD32 frame. The cast-folding fixture uses the
same current dependency instead of the removed Stmp-based runtime convention.
Both drivers are included in the core checkpoint. Their checks use explicit
failure conditions and remain active with NDEBUG.

Current validation: fixed selection passes 25,029 checks and cast folding passes
280,463 checks under UBSan/float-cast-overflow, both with and without NDEBUG.
The existing 112-driver core selection also passes; the three newly registered
drivers were run separately. This is not a full compiler build or native-device
JIT acceptance test. The new source-history ASan attempt was blocked by sandbox
LeakSanitizer process inspection and is not counted as passing.
