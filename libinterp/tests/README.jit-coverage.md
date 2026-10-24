# 64-bit JIT opcode availability checkpoint

The production sizing emitters report 175 opcode slots. `INOP` is invalid and
`IECLR` is unused, leaving 173 active commands. A native path is available for
103 active commands on AMD64, ARM64 and RV64 with M (59.5%). The other 70
currently run through the interpreter. WORD32/native-pointer and legacy
WORD64 configurations have the same opcode counts at this checkpoint.

| Backend/profile | Active commands | Native path | C interpreter path |
| --- | ---: | ---: | ---: |
| AMD64 | 173 | 103 | 70 |
| ARM64 | 173 | 103 | 70 |
| RV64 + M | 173 | 103 | 70 |
| RV64 + Zmmul | 173 | 90 | 83 |
| RV64I | 173 | 87 | 86 |

These figures count opcode availability, not complete semantic or performance
coverage. Some native paths accept a bounded subset and return to C for other
operands. For example, rounded fixed operations retain C for nonzero rounding
flags, and some floating operations retain C when the exact FP contract cannot
be preserved. Instructions may therefore contribute to both native execution
and fallback execution at runtime. No fully-native opcode count is asserted.

The 70 remaining commands are a list of current interpreter dispatch paths,
not a requirement to hand-emit 70 additional implementations. Complex C
operations already become native machine code through the platform C compiler.
A block JIT should call these helpers with a defined contract for arguments,
GC roots, exceptions, frame state and scheduler transitions. Allocation,
reference ownership, strings, channels and scheduling especially benefit from
keeping one shared, optimized C implementation. Native generation is most
useful for frequent simple operations and for removing dispatch between them.

The current 64-bit implementation remains a decoded-operation leaf JIT. The
C decoder calls a leaf for an instruction; a rejected leaf returns to the
existing C handler. This series does not implement direct helper calls from
multi-instruction native blocks. Remaining architecture work includes native
block construction, helper ABI transitions and a coordinated compiler/loader/
frame/GC pointer ABI. Finishing that work cannot be measured by counting only
opcode cases.

The later stack-lifecycle/multiply series adds `IMULB`, `IMULW` and `IMULL`
on base RV64I, raising its native-path count from 83 to 86. A subsequent exception-frame
series adds `IMULX` with its full signed 64-bit intermediate, bringing RV64I
to 87.

## Reproduce the counts

```
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/jit-coverage.py --json
```

The script compiles the actual emitters, queries every numeric opcode using
valid private layouts and the production code capacity, and checks the parsed
enum length against `MAXDIS`. JSON includes the exact supported/remaining
names for all three architectures, both WORD widths and each RV64 profile.
It requires a host C compiler, but no target SDK or Unicorn installation.
Its layout values represent valid emitter contracts; native adapter offsets
and RX publication are covered separately by `leaf64-runtime.py`.

## Current interpreter-only commands on AMD64/ARM64/RV64+M

```
IALT INBALT IGOTO IFRAME ISPAWN ILOAD IMCALL IMSPAWN IMFRAME IRET ICASE IEXIT INEW
INEWA INEWCB INEWCW INEWCF INEWCP INEWCM INEWCMP ISEND IRECV ICONSB ICONSW ICONSP
ICONSF ICONSM ICONSMP IHEADP IHEADMP ITAIL IMOVP IMOVMP ICVTFW ICVTCA ICVTAC ICVTWC
ICVTCW ICVTFC ICVTCF IADDF ISUBF IMULF IDIVF IINSC IADDC IBEQC IBNEC IBLTC IBLEC
IBGTC IBGEC ISLICEA ISLICELA ISLICEC ICVTFL ICVTLC ICVTCL ICONSL INEWCL ICASEC
IMNEWZ INEWZ INEWAZ IRAISE ICASEL ICVTFX ICVTXF IEXPF ISELF
```

The list excludes invalid/unused slots. Many listed commands should remain C
helpers when they are integrated into a native block execution path.

## This series

`ITCMP` now has a native path on all three backends, including RV64I. It
captures both native owner slots, accepts the language nil source, rejects
C-null/invalid owners to the existing VM error path, and compares immutable
heap type pointers. It changes no VM memory, reference counts, GC colors,
PCs, frame links or FP state. A private header displacement comes from the
compiler's actual Heap layout, independently of WORD width. The public Heap,
REG and Dis layouts are unchanged.

```
LEAF64_CASE=typecheck python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=riscv64 RV_FEATURE_PROFILE=i LEAF64_CASE=typecheck python3 libinterp/tests/leaf64-jit.py
REFERENCE_NATIVE=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/reference-slots.py
```

Unicorn matrices cover distinct owners with matching/mismatching type
pointers, nil/null owners, aliased slots, high native pointers, supported slot
alignments, malformed layout rejection and unchanged memory/header images.
The native AMD64 reference fixture separately checks production C behavior,
mark/reference effects and unchanged frame state on both WORD models.

Stack destruction and diagnostic validation now read frame metadata and
mapped owner slots using byte copies. The regression fixture builds four
frames across two real Stkext allocations and varies both ordinary frame
alignments, module switches and nil extension-root type descriptors. It
executes actual `freeptrs`, `memchk`, `validstk` and `destroystack`, checks exact
owner releases and newest-first extension deallocation, and preserves all
ordinary frame bytes.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/stack-unwind.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

There are 4096 unwind cases per WORD model. `UNWIND_XEC_SOURCE`,
`UNWIND_VALID_SOURCE` and `UNWIND_CHECK_SOURCE` independently select alternate
stack destructor, frame validator and mapped scanner implementations. Selecting
the parent implementations reproduces each unaligned access under UBSan.
Physical ARM64/RV64 execution and complete OS/VM acceptance remain separate.
