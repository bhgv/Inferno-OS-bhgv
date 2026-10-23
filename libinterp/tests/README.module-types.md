# Module type tables and checked calls

Run `python3 libinterp/tests/module-types.py`. The driver executes production
Loader_tnew, dtype/freetype, ten type-consuming VM paths (including direct
lookup), mcall/moduleentry and heap/list initialization bodies. Emulator error
frames are real; the bounded pool, channel constructor, current process and
module teardown are injected boundaries. No native machine code executes.

A moving realloc shim frees the old table on every successful growth. Thirty-two
descriptors are added through one Modlink and consumed through another whose
legacy type cache still names the first, freed allocation. Every descriptor is
checked in new, newz, mnewz, newa, newaz, newcmp, consmp, movmp and frame. Failed
table growth preserves the table, count and existing descriptor. Invalid,
negative and wide indices, missing tables/descriptors and negative descriptor
sizes must raise VM errors before allocation, publication or stack movement.
The channel boundary checks the descriptor and mover passed to newc; it does
not repeat the independent channels.py tests.

The call cases cover imports and named exports, interpreted and mixed compiled
flags, and actual C builtin callbacks. Invalid links must preserve the frame,
VM registers and callee reference count. Builtins must observe the callee and
then restore the caller. Tests do not execute native-PC generated code.

There are 435 cases with native WORD and 409 with a 32-bit WORD/native-pointer
model: 844 total. The latter is not a complete 32-bit ABI build. Both strict
UBSan and ASan with stack instrumentation pass:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/module-types.py
```

`MODULE_TYPES_XEC_SOURCE` substitutes an earlier xec.c without changing the
fixture. On the previous source ASan independently reports heap-use-after-free
in new after table growth, and stack-buffer-overflow in mcall when run with
`MODULE_CASES=calls`. The fallback direct lookup only lets the previous source
compile; production opcode bodies remain unmodified.

The Modlink layout is retained for existing backend offsets. Its type member
is a legacy snapshot; new VM/JIT code must use Module.type and Module.ntype.
Current native-PC backends obtain immediate Type addresses from Module.type
and delegate dynamic type operations to interpreter helpers. This test does
not establish complete malformed-bytecode validation in those compilers,
concurrent mutation safety, full GC behavior or working x64/arm64/RV64 ports.
