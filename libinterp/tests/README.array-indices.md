# Checked array addresses and 64-bit leaf indexing

Later decoded-slot/header-span and alignment checks: [README.array-slots-copy-kernel.md](README.array-slots-copy-kernel.md).

Run `python3 libinterp/tests/array-indices.py`. `CC` and `CFLAGS` select the
compiler and flags; UBSan is enabled by default. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/array-indices.py
```

Five production address instructions now share arrayaddress. It validates nil/H
owners, complete signed indices and generic descriptors before forming an
address. Zero-width generic elements return the existing data pointer directly,
including nil backing storage at nonzero indices. No null-plus-zero pointer
expression is evaluated. Nonzero-width access rejects nil/H backing storage,
byte offsets beyond the current signed-int heap extent and native address wrap.
Failures raise exBounds without changing the destination.

The helper preserves fixed opcode strides for byte, word, real and big arrays;
only indx consults the descriptor's size. A generic nil/H/negative-size descriptor
is rejected after owner/index validation. Pointer results still use the active
VM word slot: WORD32 with host pointers is an explicit scalar test model, not a
completed pointer-preserving ABI32/ABI64 bridge. The cast through uintptr makes
that existing narrowing explicit; no builtin/frame layout is silently changed.
This is not a verifier for arbitrary fabricated array storage or real allocation
ownership, which remains the heap/runtime's responsibility.

The fixture extracts actual arrayaddress, all five address instructions and lena
with real VM declarations. It runs 10,247,040 WORD64 and 10,239,360 WORD32
checks (20,486,400 total), with independent source/index/result byte offsets 0..7, including generic sizes 0/1/3/7/8/16, lengths zero through sixteen,
negative/end/full-width indices, separate output slots, result/index aliases,
result/source-pointer aliases, surrounding bytes, fixed strides with varied
ignored descriptors, invalid metadata and native wrap. All fixture records are
bounded stack storage; no GC or heap ownership acceptance is claimed.

`ARRAY_INDEX_CASE=valid`, `zero`, `invalid`, `length`, `descriptor` or `wrap`
selects a subset. `ARRAY_INDEX_SOURCE` substitutes a previous xec.c. The parent
independently produces a UBSan null-Type member-access diagnostic for descriptor
and a pointer-index-overflow diagnostic for wrap. Zero-width ordinary outputs
can match the parent; that correction is about C-definedness, not a claimed
parent crash on every empty record. Both complete word models pass ASan+UBSan
with stack instrumentation enabled. The sandbox's known error-jump warning is
not suppressed.

AMD64, AArch64 and RV64 additionally emit lena for both word widths and fixed
indb/indw/indl/indf address computation when WORD can hold a native pointer.
WORD32 address-producing leaves decline compilation and retain the interpreter
path. Generic indx remains in the helper, including zero-width descriptors.
RV64 captures source pointer slots with two word loads, preserving the existing
four-byte operand-alignment contract without requiring misaligned LD support.
Array length/data offsets are derived from the actual Array declaration during
compilation and are explicit encoder-layout inputs.

Leaves check owners before header access, reject negative lengths and invalid
indices, bound byte offsets, reject nil/H data and check address carry before
publishing an address. Rejected calls return zero with unchanged VM storage and
PC, so actual VM helpers choose the exception. No array element is read just to
compute its address, and no allocation/reference operation is introduced.

`leaf64-jit.py` checks nontrivial synthetic header offsets in all three CPU
emulators, while `leaf64-runtime.py` uses actual Array/REG offsets and executes
AMD64 code on the host. `LEAF64_CASE=array` selects just the array machine-code
matrix. Both fixtures cover high pointers, nil/H/empty headers, signed lengths,
large indices, byte offsets, address carry and exact source/index/output aliases.
The host runtime fixture also exercises actual indexing helpers on declined
operations and zero-width generic records with both word models under
ASan+UBSan. ARM64/RV64 compiler publication/unmapping and the RISC-V legacy
encoder/runtime fixtures remain covered separately.

The core VM/compiler checkpoint includes this driver. Full target boots, native
ARM64/RV64 hardware, complete frame ABI migration, relocated-PC JIT calls and
GC interaction with derived addresses are not established by these fixtures.
Tk, security/cryptography and MIPS/SPARC/PowerPC backends are unchanged.

Generic indX now also has a native WORD64 decoded-operation leaf on x86-64,
ARM64 and RV64M. Descriptor offsets are supplied explicitly by the compiler
adapter. See README.aggregate-slots.md for native-domain guards, zero-sized
records, malformed layouts and the expanded C/native verification matrices.
WORD32 retains the existing C narrowing behavior and declines this native leaf.
