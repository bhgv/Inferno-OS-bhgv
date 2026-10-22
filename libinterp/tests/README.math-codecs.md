# Math representation and array-bound regressions

Run from the repository root:

```sh
python3 libinterp/tests/math-codecs.py
python3 lib9/tests/hex-bounds.py
```

The Math fixture extracts eleven production functions and their representation
helpers. It compiles bounded array and frame models twice, with 32-bit and 64-bit
VM words, and defaults to UBSan with immediate failure. Each model checks 156
array-stride, signed-extension, bounds, IEEE representation and native sign-slot
cases. Production math.c is also syntax-checked against the repository's ARM and
x86 headers; the model is not a replacement for actual builtin ABI testing.

Six array codecs now validate nil references, element types, full-width lengths,
representation sizes and byte counts before narrowing or accessing data. Empty
arrays may have null data. Poisoned pointers in invalid descriptors ensure that
rejected inputs never reach a conversion loop. Guard bytes surround successful
outputs. External integers remain big-endian signed 32-bit values, while internal
array strides use WORD. Export intentionally retains only the low 32 bits.
Binary32 results sign-extend consistently when exposed as an integer bit pattern.

IEEE cases cover both zero signs, subnormals, ordinary values, maximum finite
values, infinities and quiet NaNs. The existing IEEE float/double assumptions
remain; this is not a floating-point arithmetic or non-IEEE portability test.
Representation unions are function-local rather than shared mutable scratch.
The lgamma fixture models a native function writing an int sign into its out
parameter; it verifies widening into the VM tuple with poisoned upper bytes.
It does not test lgamma's numerical implementation.

This sandbox cannot provide the process inspection needed by LeakSanitizer and
cannot give ASan reliable stack bounds across caught longjmps. The fixture has
no heap allocation; LSan is explicitly disabled. A supplemental GCC heap/global
ASan+UBSan configuration passes with stack instrumentation disabled:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer --param=asan-stack=0' python3 libinterp/tests/math-codecs.py
```

The default UBSan and explicit guard checks remain the primary validation.
A complete Inferno boot/build and native execution on future JIT targets are
outside this fixture. Active VM layouts and builtin signatures are unchanged.
