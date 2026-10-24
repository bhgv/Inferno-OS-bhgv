# Stack lifecycle and base RV64I multiplication

This series keeps complex VM operations in their shared C implementations.
It fixes stack frame/owner handling there and adds native paths for three
simple integer operations on RV64I.

## Stack copying and temporary-frame removal

`newstack` captures the native frame pointer from its Dis slot with a byte
copy, validates nil/null owners, and reads the source frame type without
requiring C structure alignment. Stack hints are read only after validating
both Modlink and Module owners. Stack-size validation rejects the language
nil Type sentinel before dereferencing it. All checks precede allocation and
register publication.

`unframe` captures current and source frame descriptors before changing stack
registers. It validates the current size and source owner, computes the next
SP, and releases a source extension only after all necessary metadata has
been captured. Valid ordinary frames and extension roots retain their existing
semantics. Failure leaves the registers and extension ownership unchanged.
The new extent headers allocated by `newstack`/`extend` still have natural
native alignment; the changes concern input slots and existing Dis frames.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/stack-lifecycle.py
STACK_CASES=stack python3 libinterp/tests/stack-startup.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

`stack-lifecycle.py` runs both WORD models with native pointers and executes
actual stack functions and production pointer-map initialization/cleanup.
Each model has 768 copy/allocation-failure cases, 2048 temporary-frame removal
cases 12 extent initialization/cleanup cases and 208 invalid-owner/descriptor cases
(6072 total across both models).
Inputs cover all eight alignments of source slots and ordinary frames, root
frame type fallback, multiple stack hints, retained ordinary sources, released
extensions, nil/null owners and unchanged register state on errors.
Allocation guards and exact copied frame bytes are checked separately.

`LIFECYCLE_CASE=copy`, `pop`, `extent` or `invalid` selects an independent process family.
`LIFECYCLE_SOURCE` selects another stack.c revision. The parent code reproduces
an unaligned pointer-slot read in `copy` and an unaligned Frame access in `pop`
under UBSan. Address/stack instrumentation stays enabled.

## RV64I MULB/MULW/MULL leaves

Without M or Zmmul the backend now emits a register-only shift/add loop for
`IMULB`, `IMULW` and `IMULL`. Both operands are captured before any destination
store. Logical shifts bound execution to 8, 32 or 64 multiplier bits; WORD32
operands are explicitly zero-extended for the multiplier loop. Results retain
the C interpreter's modular arithmetic and exact destination width, including
negative operands, overflow and partially overlapping slots.

M/Zmmul retain their direct hardware instruction sequences. Base RV32I is
unchanged. No native stack, helper calls, FP state, GC owners or VM frame links
are involved. The new paths fit the existing private leaf code capacity.

```
LEAF64_CASE=multiply python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=riscv64 RV_FEATURE_PROFILE=i LEAF64_CASE=multiply python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/riscv-jit.py
LEAF_RUNTIME_BACKEND=riscv64 RV_FEATURE_PROFILE=i LEAF_WORD_BITS=32 python3 libinterp/tests/leaf64-runtime.py
python3 libinterp/tests/jit-coverage.py
```

The three-architecture multiplication matrix has 26194 checks; the separate
base-I matrix has 8742. Each uses boundary/random independent arithmetic
oracles, full/partial destination aliases, neighbor guards, unchanged PCs,
code-capacity checks and complete disassembly. Base-I disassembly must contain
no MUL/DIV/REM. Publication/lifetime tests now include all three multiply
commands in the actual module, with I, Zmmul and M feature profiles.
Unicorn execution and host publication checks do not replace physical
RISC-V system acceptance. No performance improvement is claimed without
representative target benchmarks.

Base RV64I opcode availability rises from 83 to 86 of 173 active commands.
AMD64, ARM64 and RV64+M remain at 103; these operations already had native
paths there. The other commands can retain optimized C implementations when
integrated into future native blocks; opcode availability is distinct from
full block execution and the native pointer ABI transition.
