# Array copy and heap boundary regressions

Run `python3 libinterp/tests/heap-bounds.py` from the repository root.

The fixture compiles the production heap allocation, reference-map operations,
array copy/slice instructions and runtime protection with real VM declarations.
A bounded pool and destruction adapter track allocation and reference ownership.

The pointer-copy matrix enumerates all 285 valid combinations of source start,
destination start and length within an eight-element backing array. A separate
snapshot predicts every resulting pointer, and tests verify the exact reference
count of every surviving object. Views retain the real backing-root ownership;
all element, type and array references must balance after each case.

Empty non-nil source/destination arrays with nil data pointers cover both scalar
and pointer element types. Copying them must not call memory functions or allocate.
Out-of-range offsets and nil destinations still raise their original VM errors.
The previous slicela independently fails under UBSan on this empty-span case.

Existing coverage also checks heap-size limits, multiplication overflow, slice
bounds, list/link allocation limits, pointer initialization and full-WORD lengths.
For strict sanitizers:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/heap-bounds.py
```

`HEAP_XEC_SOURCE` selects alternate VM instruction bodies for comparisons.
These are bounded runtime models using native host pointers, not complete GC,
JIT/builtin ABI execution, or an Inferno boot. LeakSanitizer process inspection
is disabled in the sandbox; ASan/UBSan remain active.

Nil array slices now enforce the language bound invariant documented in
[Limbo expressions](../../doc/limbo/limbo.ms): only `[0:0]` succeeds.
Equal negative or positive nonzero bounds previously bypassed all checking,
including native-WORD extrema. Both H and native nil forms are covered;
invalid equal bounds now raise exBounds without allocation or destination changes.
Unequal bounds retain the existing exNilref status. This intentionally corrects
the earlier fixture's accepted `nil[INT_MIN:INT_MIN]` legacy behavior.
The parent slicea body fails the extended rejection assertions.

Empty and zero-sized-element views retain a native nil data address without
performing C pointer arithmetic on it. Sixteen repeated-slice scenarios check
root transfer, collapse of nested views, exact type/root references and injected
allocation failures that preserve the original view. This is a C-definedness
correction; ordinary successful output comparisons alone need not distinguish
the parent null-plus-zero expression. The pool/destructor adapters do not run GC.

The complete heap fault suite passes strict UBSan. In this sandbox its additional
nested siglongjmp paths trigger ASan's ignored __asan_handle_no_return warning
followed by a stack-redzone report; that full ASan run is not accepted as passing.
Keep stack instrumentation enabled and select successful empty-view construction
separately for ASan/UBSan:

```
HEAP_CASE=empty-views CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/heap-bounds.py
```

The subset executes all sixteen successful empty-view/root transfers; it does not
claim allocation-failure or nil-error sanitizer coverage. Those scenarios remain
in the complete UBSan suite and ordinary host execution.
