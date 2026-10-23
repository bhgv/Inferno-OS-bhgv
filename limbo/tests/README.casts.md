# Constant cast folding regressions

Run from the repository root:

```sh
python3 limbo/tests/fold-casts.py
CFLAGS='-O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer' python3 limbo/tests/fold-casts.py
```

The fixture compiles production cast folding, fixed opcode selection and VM
fixed conversion bodies. It passes 280,463 checks under UBSan including
float-cast-overflow and ASan+UBSan. Minimal AST/type models supply scale lookup,
source locations, equality, string parsing and diagnostics. Real instruction
numbers come from isa.h. Formatted-string allocation and complete compiler
parsing/code emission remain outside this host fixture.

Integer int/byte/big sources retain canonical signed-32 or byte narrowing before
conversion. Real-to-integer folding retains the interpreter's half-away-from-zero
expression and rejects rounded native intermediates outside the destination's
representable domain. Rejected folds retain the source constant's complete type,
location and value. Existing signed boundaries, string inputs and nextafter
neighbors around signed-64 limits remain covered.

Fixed integer conversions now evaluate the exact selected WORD32 cvtxx/cvtxx0/
cvtxx1 bit steps rather than assuming floating-point rounding matches them.
Arithmetic right shifts, finite-width left shifts, coefficient division and
rounding flags match production VM instructions. Fixed-to-big follows codegen's
real precast and half-away rounding. Int/byte-to-fixed uses fixed instructions;
big-to-fixed follows its real precast. Valid conversions remain compile-time
constants, including fixed-to-fixed casts. The removed real2fix/fix2fix helpers
performed unchecked native casts and could not model these distinct paths.

All 64 combinations of eight scales are tested for each input -1024 through
1024, comparing the folded result directly with the selected production WORD32
VM instruction. Additional fixed source and int/byte/big target cases check
negative fractions, integer narrowing and preservation of same-type constants.

Real and string fixed targets use reciprocal multiplication, matching the factor
emitted by codegen, before half-away rounding and signed-32 range validation.
Eight target scales include very small/large values and a subnormal scale.
Finite cases, ties, NaN/infinity and boundary values compare folding against the
actual real-to-fixed VM function. Invalid folds retain the source AST and remain
runtime expressions; contexts requiring a constant may reject such inputs.
Production parsing of strings and target libmath are not modeled here.
The separate fold-strings.py and fold-string-length.py fixtures cover actual
string parser dependencies, numeric prefixes, portable deferral of wide int/byte
prefixes, and length folding across embedded NULs; see README.fold-strings.md.

Baseline tests reproduce mismatched fixed rounding and native float-cast-overflow
in real-to-fixed execution. This suite uses a WORD32 scalar model with native
pointers and minimal registers; it does not establish a new Dis ABI or validate
native JIT calling conventions. Full optimized compiler builds and Inferno boot
remain outside scope. ASan stack instrumentation is enabled, including the local
setjmp VM-error fixture. LeakSanitizer inspection is disabled for the sandbox;
this fixture allocates no dynamic heap.
