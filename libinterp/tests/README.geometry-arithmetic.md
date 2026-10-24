# VM and native drawing coordinate arithmetic

Run `python3 libinterp/tests/geometry-arithmetic.py` from the repository root.
`CC` and `CFLAGS` choose the host compiler and flags; strict UBSan is the default.
For AddressSanitizer as well:

```
CFLAGS='-O2 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/geometry-arithmetic.py
```

Ten VM Point/Rect arithmetic methods now evaluate addition, subtraction and
multiplication with UWORD intermediates, as the VM integer instructions do.
Point.div preserves exZdiv before either result coordinate is written and uses
unsigned negation for divisor -1, including the signed minimum. Seven libdraw
helpers apply the corresponding unsigned-int arithmetic and safe negation to
native int coordinates. libdraw divpt retains its existing nonzero-divisor
precondition; no new error policy is introduced for native C callers.

VM coordinate width follows the active WORD/UWORD declarations. Native libdraw
coordinates remain int-sized. No truncation to a proposed future Dis ABI, target
JIT register width, or fixed four-byte pointer convention is introduced. The
resulting wrap behavior removes host signed-overflow optimization and traps from
these operations, consistently with the current interpreter integer arithmetic.
Language integer-overflow results were already unspecified; the documentation
is not changed to promise a new language-wide arithmetic contract.

The fixture compiles complete production geom.c and libdraw/arith.c bodies using
real interpreter, Draw point/rectangle and generated builtin-frame declarations.
Only the VM error boundary and unrelated format entry points are adapters; the
formatting adapter aborts if reached. Python's arbitrary-precision integers compute
expected coordinate bits independently of the production formulas' C evaluation.
Signed division is calculated with integer magnitudes and truncation toward zero,
without floating point. Each width uses 256 signed/unsigned boundary combinations
and 512 deterministic random vectors, including noncanonical output rectangles.

There are 21,504 VM result/error checks per WORD model and 5,360 native libdraw
checks, totaling 48,368. Each rectangle check compares both corners, and each
point check compares both coordinates. Result slots are separate or exact aliases
of the relevant input point/corner/rectangle. Zero VM divisors must preserve both
old output coordinates. Full-width minima, maxima, opposite signs, dimensions,
translation, inset, multiplication and minimum/-1 are covered. Both complete
models pass ASan+UBSan with stack instrumentation enabled. The known sandbox
warning at longjmp is not suppressed. This fixture has no heap allocations.

`GEOMETRY_SOURCE` and `DRAW_ARITH_SOURCE` can select alternate production files.
`GEOMETRY_CASE=vm` or `native` selects a layer, and `GEOMETRY_OP` selects one C
entry-point name. With both source variables pointing to the parent revisions,
`GEOMETRY_PROBE_PARENT=1` requires a UBSan failure independently for each of the
ten VM methods in both models and the seven native helpers. All 27 probes fail
on signed overflow or minimum/-1 as expected; a generic process failure is not
accepted as a successful reproduction.

The shared `slice-tokenize-suite.py --core` checkpoint now includes this arithmetic
fixture without a display server or Tk dependency. Adjacent draw-image/windows
fixtures check existing allocation and window behavior. The compiler is covered
by the checkpoint's existing folding and case-dispatch tests; compiler source is
unchanged in this series. These host fixtures do not run a full Inferno VM,
window server, GC cycle, generated native builtin calls or target JIT code.
WORD32 retains native host pointers and is not a complete ABI32 implementation.
Tk, security/cryptography and all backend sources are outside this series.
