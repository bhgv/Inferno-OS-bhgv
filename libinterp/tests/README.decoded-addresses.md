# Native-safe generated address decoding

The shared decoder now captures each complete instruction with `memmove` before
reading operand fields. Immediate operands still refer to the original Inst
cells, preserving branch-pointer and decoded-operation semantics. No temporary
snapshot address escapes the decoder.

Direct FP/MP addresses use checked native unsigned arithmetic with signed WORD
offsets. Negative offsets remain supported, including the minimum signed WORD
without a signed negation overflow. Nil/H bases, address underflow and carry
raise the normal VM errors before a derived address is used.

Indirect addressing copies the native pointer cell with `memmove` at any byte
alignment, checks the complete cell span, then applies the second offset through
the same address helper. The former `*(uchar**)` expression could perform an
unaligned C read before either native JIT or C handlers received their operands.
These checks validate numeric spans, not allocation membership or mapped pages.

The VM loop fetches opcode/addressing bytes without typed reads through a
possibly unaligned Inst pointer. It checks the instruction span first and
advances PC by the byte stride. Both native fast and complete C-tail execution
retain their existing selection and scheduling behavior.

`decgen.c` and `dec.c` are updated together. All 256 functions have comments and
human-readable spacing. Private function names now use `DisDecodeXX`, avoiding
the old `D2B` collision with the allocator's macro in combined builds. The public
`dec[]` table and its byte indices are unchanged. Shared helpers live in
`dec-addr.h`, which is an explicit build dependency. GNU/Clang builds inline the
helpers; other C compilers retain ordinary static definitions. An optimized GCC
assembly check found no calls to the helpers or to memmove in generated decoder
functions; this is not a throughput benchmark.

## Native generator and checks

`sh makemk.sh` now installs native `mk`, `iyacc` and `decgen`. The integration
driver verifies that running the real generator exactly reproduces the committed
`dec.c`, then compiles the generated decoder under strict native diagnostics.
It also retains native ELF checks, rejected-compiler preservation, real Limbo
grammar generation and compilation of all 17 compiler translation units.

```sh
python3 libinterp/tests/decoded-addresses.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/decoded-addresses.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 utils/tests/native-bootstrap.py
python3 libinterp/tests/vm-widths.py
python3 libinterp/tests/dis-numeric-literals.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

Results on the AMD64 host:

| Check | Per WORD model | Both models |
| --- | ---: | ---: |
| Complete REG and unchanged instruction images | 98304 | 196608 |
| Exact decoder errors with balanced Proc frames | 35 | 70 |
| Real decoder to C/native RX MOVW executions | 128 | 256 |
| Existing complete C/RX dispatch routes | 177152 | 354304 |
| Compiler checks | 5002 | 10004 |
| VM-loop integration checks | 295 | 590 |

The image matrix covers all 256 addressing bytes, six middle-register boundary
values, eight instruction alignments and eight frame/pointer-cell alignments.
Unused fields retain their previous values. The decoder-to-execution fixture
copies a signed WORD through a genuinely unaligned native pointer cell into an
unaligned result, using the production MOVW handler and emitted executable code.
The VM-loop matrix adds eight unaligned-PC successes and 25 invalid-PC errors
per WORD model. Combined ASan/UBSan passes; checks remain active with NDEBUG.

The prior code independently fails four decoder families (unaligned pointer
cells, unaligned instruction images, invalid address errors, and decoder-to-RX
execution) in both WORD models. The prior VM loop also fails unaligned-PC
execution in both models: ten reproduced parent failures. Replay uses
`DECODE_SOURCE`, `DECODE_CASE=images|errors|native`, `DECODE_FRAME_ALIGN`,
`DECODE_IMAGE_ALIGN`, `NUMERIC_WORD_BYTES=4|8`, `BRIDGE_XEC_SOURCE` and
`BRIDGE_WORD_BITS=32|64`. The numeric fixture borrows the existing parser/type/error
boundary scaffolding; the generated decoder, handlers and native emitters are
production code.

The native numeric/decoder driver selects AMD64 or ARM64 from its actual host;
only AMD64 machine-code execution was available here. ARM64 compiler binding
still passes 5002 checks per model. A real ARM64 run remains required. Neither
scalar WORD model is a complete VM ABI port or a booted Inferno system.

## Opcode availability

| Backend | Complete JIT + C | Guarded native fast paths | C-only |
| --- | ---: | ---: | ---: |
| AMD64 | 173/173 (100%) | 166/173 (95.95%) | 7 |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) | 7 |

See the [per-command table](README.jit-readiness.md). Native availability is not
complete coverage of operand domains. The unresolved compiler/runtime frame and
reference-cell layout contracts remain documented in
[the native tool report](../../utils/tests/README.native-bootstrap.md).
RV64 work is deferred; Tk, crypto/security and MIPS/SPARC/PowerPC JIT sources
are unchanged.
