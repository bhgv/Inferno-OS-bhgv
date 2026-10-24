# WORD32 address leaves and UTF result publication

AMD64, ARM64 and RV64 leaf backends now implement `lea`, `indb`, `indw`,
`indl`, `indf` and `indx` with WORD32 operands and native 64-bit pointers.
They already supported the corresponding WORD64 model. The emitted code
matches the existing C interpreter contract:

* Owners and backing pointers remain native pointers.
* Array lengths and indices use the configured signed WORD width.
* `indw` uses that WORD width as its element stride; real and big use eight.
* Bounds, descriptor sizes, byte-offset limits and native carry are checked
  before narrowing the computed address into the destination WORD.
* Only the destination WORD bytes change, even when slots overlap.
* Zero-sized descriptor elements preserve the backing identity without
  dereferencing it. Invalid owners still defer to the C error path.

`lea` never dereferences its source address. RV64 fixed-stride operations and
`lea` work with base I; descriptor multiplication still requires M or Zmmul.
ARM64 uses signed WORD metadata loads without increasing the descriptor leaf's
instruction count. Native pointer slots in the Dis frame remain four-byte
aligned where required by the current ABI; RV64 operand helpers avoid LD/SD
on those slots.

This does not widen Dis address slots or complete a new 64-bit language ABI.
For WORD32 the existing C semantics truncate high pointer bits. The tests
verify that exact behavior; they do not make a truncated address suitable for
later native dereference. A complete native pointer ABI still requires a
coordinated compiler, loader, frame and GC transition.

Run production emitters with Unicorn and Capstone available:

```
LEAF64_CASE=address python3 libinterp/tests/leaf64-jit.py
LEAF64_CASE=array python3 libinterp/tests/leaf64-jit.py
LEAF64_CASE=descriptor python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=riscv64 RV_FEATURE_PROFILE=i LEAF64_CASE=array python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/riscv-jit.py
```

The shared fixture checks both WORD widths, independent narrowing/bounds
oracles, high pointers, nil owners, negative lengths, index wrapping, carry,
aliases, untouched neighboring bytes, unchanged PCs and code-buffer bounds.
AMD64/ARM64 `lea` also covers every byte alignment; RV64 uses the supported
four-byte slot alignment and checks all memory accesses for natural alignment.

Actual native AMD64 descriptor execution now covers both WORD widths, using
real Array/REG declarations and the independent production C interpreter for
comparison (85248 descriptor cases per model). The address fixture publishes
both `movpc` and `lea` and checks native and C effects independently.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/leaf64-runtime.py
LEAF_WORD_BITS=32 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/leaf64-runtime.py
MOVPC_NATIVE=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/vm-addresses.py
LEAF_RUNTIME_BACKEND=arm64 LEAF_WORD_BITS=32 python3 libinterp/tests/leaf64-runtime.py
LEAF_RUNTIME_BACKEND=riscv64 RV_FEATURE_PROFILE=zmmul LEAF_WORD_BITS=32 python3 libinterp/tests/leaf64-runtime.py
```

The host publication tests validate executable-page ownership and fallback
entries for ARM64/RV64; they do not execute those backends on physical CPUs.

`Sys.utfbytes`, `Sys.byte2char` and `Sys.char2byte` now capture their results
and publish with byte copies, removing unaligned WORD/tuple stores. Null or
sentinel return addresses raise before an encoder can alter its byte array.
Unicode decoding, incomplete-sequence handling and malformed-sequence policy
are unchanged. A short malformed sequence remains governed by the existing
`fullrune` policy; the regression fixture supplies enough bytes when checking
the complete malformed-sequence path.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-utf.py
SYS_UTF_CASE=alignment python3 libinterp/tests/sys-utf.py
SYS_UTF_CASE=badreturns python3 libinterp/tests/sys-utf.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

The UTF driver retains exhaustive scalar encoding/decoding against an
independent UTF encoder. New tests cover every result alignment, ASCII,
multibyte, surrogate replacement, Unicode limits, valid/incomplete/malformed
decodes, prefix counting, exact result widths and guard bytes. Bad-return
cases check that byte arrays stay unchanged. Both WORD models pass with
ASan/UBSan. `SYS_UTF_SOURCE` selects an older runtime; the new alignment
family reproduces its misaligned WORD store under UBSan.
