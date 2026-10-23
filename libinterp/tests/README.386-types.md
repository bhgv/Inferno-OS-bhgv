# 386 helper buffer sizes and byte emission

Type helper scratch space is now derived from the pointer-map length, with
bounded allocation arithmetic. Initializer and destructor bytes are emitted
privately and synchronized before either helper pointer is installed. The
old fixed 4096-byte scratch buffer overflowed on large dense maps.

`genw` now writes four little-endian bytes explicitly, and large ModRM
displacements use it. This removes unaligned C word stores and keeps emission
at the target's 32-bit width independently of the host word size.

Run from the repository root (requires Python `unicorn`):

```sh
python3 libinterp/tests/386-types.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/386-types.py
```

The fixture extracts production word/ModRM/constant/branch encoders and both
helper emitters. It models target `sizeof(WORD*)` as four bytes, emits code for
seven map lengths up to 4096 bytes, and executes 21 dense/empty/random
initializers in Unicorn. It compares every destination byte with an independent
pointer-map expansion, including frame canaries. Allocation and flush failures
add 63 checks (84 map scenarios total). Separate probes check word and large
ModRM displacement encoding at eight byte alignments, preserving canaries.
The executable runs under ASan/UBSan; the ctypes library omits ASan to avoid
loading its runtime into Python. LeakSanitizer process inspection is disabled.

`X86_TYPE_SOURCE` selects another backend source. With pre-fix sources,
`X86_TYPE_CASES=words` and UBSan reproduce the misaligned store;
`X86_TYPE_CASES=large` and ASan reproduce the 4096-byte buffer overrun.
The initializer is actually executed, but destructors are only generated:
external calls, reference counting, GC and native calling-convention integration
remain separate tests.
