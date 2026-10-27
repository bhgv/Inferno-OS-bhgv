# Sentinel targets in all direct branch families

JMP and taken integer, real and string comparisons validated their target cell
but accepted a contained nil/H as the next PC. This could publish an unusable
control-flow identity before the next interpreter fetch failed.

All 31 operations now reject those identities before PC publication. The C JMP
macro uses a shared byte-safe branchpc routine; real branches use that same path,
and string branches pass their existing exHeap failure identity explicitly.
Integer/real/JMP retain exBounds. Both priority JIT emitters validate the captured
target after the predicate is taken and before their first VM store.

False comparisons continue to ignore their target cells, including nil, H and
wrapping/unmapped cells. Native pointer width stays independent of the VM WORD
width. Other native target tokens retain their representation. The tests include
opaque high-address tokens for construction only; no synthetic destination is
fetched or executed. This series adds no allocation-membership or executable-map
verification. Indexed Dis target extents are covered separately by
[README.pc-target-boundaries.md](README.pc-target-boundaries.md).

The AMD64 string path captures the target from r10 into rax before validation,
leaving rdx available as guard scratch. Source characters, comparison results and
target cells are captured in order before publication. ARM64 retains its captured
x12 target while using the existing guard scratch register.

## Validation

Run from the repository root; guest tests require Unicorn:

```sh
python3 libinterp/tests/direct-target-native.py
python3 libinterp/tests/direct-target-jit.py
python3 libinterp/tests/integer-branch-native.py
python3 libinterp/tests/branch-slot-native.py
python3 libinterp/tests/integer-branches.py
python3 libinterp/tests/real-branch-native.py
python3 libinterp/tests/real-branches.py
python3 libinterp/tests/string-branches.py
python3 libinterp/tests/string-compare-spans-c.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-dispatch-jit.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The native matrix borrows the real branch helper, operations, stringcmp, compiler
and cjitexec. It uses actual REG/String layouts and executable complete entries.
Direct C, fast-plus-C and complete JIT entries each run 318,080 full-image routes
per WORD model, giving 1,908,480 total routes under ASan/UBSan and NDEBUG.
These include 629,760 VM errors and 440,064 successful false comparisons with
poisoned unused target cells. Error-route topologies run in separate processes;
ordinary stack/address instrumentation remains enabled.

All 18 byte/int/big predicates, six real predicates, six string predicates and
JMP are exercised. Inputs include finite values, quiet NaNs, empty strings,
ASCII and Rune strings. Every source/target byte shift, a PC-aliasing target cell,
nil/H contents, normal/opaque targets and poisoned target-cell addresses are
covered. The oracle independently computes source order and unordered real
predicates, then compares the entire REG and data image. NaNs exercise the C
fallback through real executable JIT entries, including NE's taken target error
and other predicates' false-target nonaccess.

The separate guest matrix encodes independent REG and String fields with two
layout biases and native data addresses above 4 GiB. It checks 178,920 cases on
each architecture (357,840 total), including 77,688 fast declines per architecture
from invalid targets or required NaN fallback. False branches ignore 41,256
poisoned targets per architecture. Every refused/untaken operation has an empty
write trace; a successful taken operation performs exactly one native PC store,
including an aliased same-value store. Host stack, callee-saved registers and FP
controls/status remain unchanged. Sizing, insufficient-capacity and output-suffix
checks also remain active with optimized Python.

Existing regression matrices pass:

| Matrix | WORD32 | WORD64 |
| --- | ---: | ---: |
| Native integer branch C/RX images | 204,240 | 205,008 |
| Integer C values/alignment | 1,557,512 | 1,557,512 |
| Integer slot/target preflight | 4,504 | 4,888 |
| Native real FP/control/alignment | 497,664 | 497,664 |
| Real C PC/alignment/FP state | 50,752 | 50,752 |
| String Unicode/alignment/PC | 438,366 | 438,366 |
| String slot preflight | 1,320 | 1,320 |
| String comparison span/identity/error | 156,672 | 156,672 |

Existing branch fixture drivers now borrow the production jump macro and helper
as well as the operation bodies. Their old fixture-local macros no longer hide
the new publication check. Parent-source substitution remains supported.

Twenty-four isolated parent-fa473eb probes reproduce the defect: BEQB, BEQW,
BEQL, BEQF, BEQC and JMP through C under both WORD models, and each representative
emitted independently on AMD64 and ARM64. DIRECT_TARGET_SOURCE,
DIRECT_TARGET_WORD_BITS, DIRECT_TARGET_ROUTE=0 and DIRECT_TARGET_OP select native
probes; DIRECT_TARGET_EMITTER_DIR and DIRECT_TARGET_ARCHES select guest probes.
Representative operation indices are 0, 6, 12, 18, 24 and 30 respectively.

Complete dispatch still passes 177,152 native routes, 5,002 compiler and 295 VM
integration checks per WORD model. ARM64 compiler preflight passes 5,002 per
model; both guest complete-tail matrices pass 16,881 cases. Native execution in
this series was AMD64; ARM64 instructions were tested in Unicorn.

## Port status

AMD64 and ARM64 retain 173/173 complete entries including C and 166/173 guarded
native fast paths. These are opcode availability counts. The full native ports
remain unfinished: WORD/reference-cell widths and the compiler/runtime frame
layout still disagree, as measured in
[README.pc-target-boundaries.md](README.pc-target-boundaries.md). Full VM/Limbo
boot and physical ARM64 acceptance remain outstanding. RV64 emitter work, Tk,
security/crypto and MIPS/SPARC/PowerPC JIT sources are outside this series.


The standalone `leaf64-runtime.py` fixture also borrows the production `JMP`
macro and `branchpc` body before extracting `branchreal`. Run it with default
WORD64 and `LEAF_WORD_BITS=32`; omitting those dependencies fails compilation
before the native diagnostic can exercise either real-branch path.
