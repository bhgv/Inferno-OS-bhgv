# Bounded-stack sorting and JIT table integration

Run from the repository root:

```sh
python3 libinterp/tests/sort-stack.py
python3 libinterp/tests/math-vectors.py
JIT_TABLE_NATIVE_SORT=lib9 python3 libinterp/tests/jit-tables.py
JIT_TABLE_NATIVE_SORT=libkern python3 libinterp/tests/jit-tables.py
```

Both native qsort copies now recurse on their smaller partition and iterate on
the larger one. Native public entry points reject empty, nonpositive-width or
signed-span-overflow requests before multiplying counts or accessing storage.
The production Math index sorter likewise recurses only on the smaller
partition. Partition comparison, equal-value grouping and NaN ordering remain
unchanged. The stack bound is logarithmic even on very unbalanced partitions;
worst-case comparison time can still be quadratic.

`sort-stack.py` compiles production qsort from lib9 and libkern, plus production
Math comparison, swapping, pivot and partition bodies in WORD32 and WORD64
models. Call-boundary wrappers count recursion without replacing partition
operations. Each of the four configurations runs 19,798 scenarios:

* An adaptive rank generator constructs a fixed permutation separately for each
  algorithm at 127, 512, 2048 and 4096 elements. After freezing keys, sorting
  restarts from the original index order with ordinary numeric comparisons.
  Sortedness, permutation membership and a logarithmic recursion bound are
  checked independently. Old native bodies use 67 levels at 127 elements; old
  Math uses 34. Fixed bodies use two on those same adversaries.
* Ordinary ordered, reversed, duplicate and random inputs cover lengths 0..128.
  Math also checks NaNs, infinities and both zero signs.
* Records of widths 1..24, offsets 0..7 and lengths 0..32 exercise byte and
  aligned native-word swaps. Whole-buffer comparison against independent
  insertion sort checks record contents and guard bytes.
* Native invalid/empty requests include signed count extremes, zero/negative
  element widths and overflowing products. They must not touch data or call
  the comparator.

The JIT preflight fixture retains its default host-sort adapter and can now
link either real native sorter instead. Its 98,369 checks include plans with
8..4096 tables in sorted, reversed, shuffled and duplicate order, overlapping
or conflicting-kind spans, ignored instructions and invalid alignment.
Input data/instructions and allocation balance are checked after validation
and injected allocation failure. Native sorting runs in a separate translation
unit, retaining host native counts while the preflight uses its existing
32-bit VM model. This tests the real validation/sort boundary, not generated
ARM/386 machine code or a native 32-bit ABI.

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 libinterp/tests/sort-stack.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' JIT_TABLE_NATIVE_SORT=libkern python3 libinterp/tests/jit-tables.py
```

`SORT_LIB9_SOURCE`, `SORT_LIBKERN_SOURCE` and `SORT_MATH_SOURCE` select prior
implementations. `SORT_CASE` selects `native`, `math`, `ordinary`, `records`, or
`invalid`. Each old sort independently fails the fixed-key adversarial depth
check. Tests default to strict UBSan; supplementary ASan runs also pass.
Full Inferno boot, end-to-end Limbo compilation, generated-code ABI behavior
and future x86-64/AArch64/RV64 backends remain outside these host fixtures.
