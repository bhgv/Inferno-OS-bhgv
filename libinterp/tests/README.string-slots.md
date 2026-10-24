# String slots and RISC-V multiplication/division capabilities

The string series covers `slicec`, `addc`, `cvtca`, `cvtac`, `cvtcw`, `cvtcf`,
`cvtwc`, `cvtlc`, and `cvtfc`. Each captures scalar values or owners using
`memmove`; pointer results are published before the replaced owner is released.
The slice instruction validates complete signed VM words before converting to
the public `slicer(ulong, ulong, String*)` API. The shared slice, concatenation,
duplication and C-string conversion functions reject null owners; slice,
concatenation and duplication reject INT_MIN representation lengths before
negation. Valid empty-string (`H`) behavior remains supported.

Run:

```
python libinterp/tests/string-slots.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python libinterp/tests/string-slots.py
python libinterp/tests/slice-tokenize-suite.py --core
```

`string-slots.py` extracts actual production functions, reuses guarded codec
allocations and real VM declarations, and runs WORD64 and WORD32 models with
native host pointers. It checks every byte alignment, exact owner aliases,
allocation failure before replacement, ASCII/rune parsing, numeric formatting,
string/byte-array conversion and all slices of a four-character ASCII/rune
string (including invalid bounds and H). Bytes outside the result slot and all
heap guards and reference counts are checked. There are 78,471 counted checks
per word model. The parent revision fails UBSan on an unaligned owner read.

The new slot driver passes ASan/UBSan with stack instrumentation enabled.
The complete older `string-codecs.py` ASan run still reports stack redzones
following `longjmp`/`__asan_handle_no_return` in this sandbox, including on the
parent revision. It is not counted as passing ASan; its full UBSan run passes.
LeakSanitizer process inspection is disabled by the existing fixture hook.

RISC-V uses separate private layout fields for multiplication (`hasm`) and
integer division (`hasdiv`). The compiler adapter derives them independently
from `__riscv_mul` and `__riscv_div`. Zmmul must not produce DIV/REM instructions:

* multiplication, powers, MULX and descriptor indexing keep their native leaves;
* byte/word/long DIV/REM and DIVX decline compilation without division support;
* the existing C dispatch handles declined operations.

The compiler macro distinction is documented in the upstream implementations:
https://github.com/llvm/llvm-project/blob/main/clang/lib/Basic/Targets/RISCV.cpp
https://gcc.gnu.org/pipermail/gcc-patches/2021-October/582664.html

Run `riscv-features.py` for actual preprocessor fragment profiles and
`riscv-jit.py` for the encoder matrix. The latter checks that Zmmul emits the
same multiplication code as M and rejects all seven division/remainder leaves.
`leaf64-runtime.py` accepts `RV_FEATURE_PROFILE=i`, `zmmul`, or `m` together
with `LEAF_RUNTIME_BACKEND=riscv64`; optionally set `LEAF_WORD_BITS=32`.
These test actual compiler publication, rejection tables, allocation failures,
RX permissions and teardown. They do not execute RISC-V on the host.
Unicorn/Capstone tests execute and decode the existing x86-64, arm64 and RV64
frame arithmetic and descriptor leaves. No hardware arm64/RV64 boot is claimed.
