# Math vector, permutation and matrix regressions

Run from the repository root:

```sh
python3 libinterp/tests/math-vectors.py
```

The fixture compiles production Math wrappers and sorting helpers together with
libmath/blas.c and libmath/gemm.c in bounded WORD32 and WORD64 array/frame models.
Each model checks 2,344 cases under UBSan with immediate failure. These models
exercise widths and bounds; actual builtin ABI execution and a complete Inferno
build are outside this fixture. Production files are separately syntax-checked
against the repository's ARM and x86 headers.

Sorting covers lengths 0 through 257, ascending/descending/equal/duplicate values,
and NaNs. It checks complete WORD index slots, permutation preservation, sorted
values, unchanged real arrays and index-buffer guards. NaNs are ordered after
numeric values; equal values and NaNs need not retain their original order.
Every index is checked before mutation, including full-word out-of-range indices.
Invalid references, types and lengths fail before poisoned data can be accessed.
An empty permutation needs no pointer arithmetic on its null backing buffer.

Vector tests cover dot, maximum-magnitude index, absolute-value sum and Euclidean
norm, including empty arrays and independent validation of the second dot operand.
Norm2 uses scaled sums of squares: representable results around 1e308 and 1e-300
must not overflow or disappear through intermediate squares. Finite results are
compared with hypot; NaNs propagate and repeated infinities remain infinite.

GEMM tests compare every N/T/C transpose combination for small rectangular shapes,
zero inner dimensions, padding, alpha zero and beta zero against a direct triple
loop. They check the existing nil-left square-identity extension, no reads of old
NaN C values when beta is zero, absent operands for empty products, and empty
output shapes. Bounds must include the final active row of the final column;
large strides are multiplied only after widening. Dimensions, transpose controls,
strides and array lengths are checked before conversion to native int parameters.
A nil right matrix is rejected when the product is active. Invalid arguments leave
C untouched. The identity extension requires m == k when active.

Baseline probes reproduce WORD64 permutation corruption, norm overflow, beta-zero
NaN contamination in the identity path, and incomplete final-column bounds.

The sandbox's process-inspection and longjmp stack-bound limitations also apply
here. LeakSanitizer is disabled. A supplemental GCC heap/global ASan+UBSan run
uses the same explicit stack-instrumentation restriction as the other VM fixtures:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer --param=asan-stack=0' python3 libinterp/tests/math-vectors.py
```

The default UBSan, numeric oracles and explicit data guards are primary validation.
