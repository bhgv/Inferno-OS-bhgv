# RV64I program addresses and tokenizer publication

The RISC-V `IMOVPC` leaf now works without M or Zmmul. It retains the direct
MUL sequence when multiplication is available. The RV64I path accumulates the
bounded index product in a register-only loop of at most eleven iterations.
It preserves the Module register, checks owners and signed program length
before loading the index, and checks native address carry before publishing.
It does not use native stack space, helper calls or floating-point state.

Run from the repository root with Unicorn and Capstone available:

```
LEAF64_CASE=movpc python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=riscv64 RV_FEATURE_PROFILE=i LEAF64_CASE=movpc python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/riscv-jit.py
LEAF_RUNTIME_BACKEND=riscv64 RV_FEATURE_PROFILE=i python3 libinterp/tests/leaf64-runtime.py
LEAF_RUNTIME_BACKEND=riscv64 RV_FEATURE_PROFILE=zmmul python3 libinterp/tests/leaf64-runtime.py
```

The shared leaf matrix retains AMD64/ARM64/RV64 owner, alias and carry tests.
For both RV64 WORD models it additionally checks every stride from 1 to 2047:
matching sizing passes, bounded writes, undersized-buffer rejection, complete
disassembly, zero/large indices and address carry. The RV64I matrix rejects
any emitted MUL/DIV/REM instruction. This tests production emitted machine
code in Unicorn and publication/lifetime on the host; it does not boot a
RISC-V operating system or establish a complete native 64-bit VM ABI.

`Sys.tokenize` now compares byte characters as unsigned values in both source
and delimiter strings, so mixed byte/rune representations agree. It rejects
C-null strings and INT_MIN rune lengths before allocating partial words.
The language nil source still returns an empty result without reading its
delimiters. Null/sentinel result addresses are rejected before construction.
Tuple fields are captured/published with byte copies before releasing the old
list. Their native field offsets remain identical to the generated builtin
header; this does not change the Dis tuple or compiler pointer ABI.

```
TOKENIZE_CASE=alignment CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-tokenize.py
TOKENIZE_CASE=membership CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-tokenize.py
TOKENIZE_CASE=malformed CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-tokenize.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

Each tokenizer family runs with WORD32/native pointers and legacy WORD64,
and with actual emu and os error-protection functions. Alignment tests cover
128 high-byte characters, both source/delimiter representations and all eight
result alignments (4096 cases per configuration). Membership tests compare all
256 character/delimiter pairs in both delimiter representations (131072 cases
per configuration). Six malformed-input cases verify unchanged output,
zero allocations and balanced error stacks. Existing randomized content and
allocation-failure matrices remain enabled in the default/core runs.

`TOKENIZE_SOURCE` can select the parent runtime: `alignment` reports an
unaligned tuple access, `membership` fails its independent equality oracle,
and `malformed` reports a null string access. These are regressions against
the previous implementation, rather than tests that only mirror new code.
