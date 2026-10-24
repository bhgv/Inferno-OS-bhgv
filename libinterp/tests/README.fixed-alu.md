# Fixed-scale leaves and integer operand alignment

The x86-64, ARM64 and RV64 decoded-operation JITs now implement ICVTXX for
signed scales -63 through 63. They validate the complete scale before reading
the source. Negative scales use an arithmetic right shift; scales outside the
range return zero without writes so the VM retains invalid-scale exceptions.
Even WORD32 uses a signed 64-bit intermediate before truncation. RV64 needs only
RV64I; RV32 declines this opcode. No frames, GC maps or native call ABI changes.

The C cvtxx handler captures unaligned WORD source/scale slots and publishes only
a successful result. Eighteen add/subtract/multiply/and/or/xor handlers similarly
capture both inputs before an overlapping write. Unsigned WORD/LONG arithmetic
keeps wrapping behavior independent of signed-overflow optimization.

Run the dedicated drivers from the repository root:

- `python libinterp/tests/integer-alu.py`: 2,336,256 cases per WORD model.
- `python libinterp/tests/fixed-scale.py`: 1,170,432 cases per WORD model.
- `LEAF64_CASE=fixed python libinterp/tests/leaf64-jit.py`: 44,154 checks across
  x86-64, ARM64 and RV64 with both VM word sizes, including inaccessible source
  on rejected scales, aliases and both directions of partial overlap.
- `python libinterp/tests/riscv-jit.py`: 2,050 RV32/RV64 checks, including RV64I
  support and RV32 rejection for the new opcode.

Unicorn/Capstone must be on PYTHONPATH for emitter execution tests. VM drivers
use UBSan by default. Both word models pass with
`CFLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"`.
Only leak process inspection is disabled; stack checking remains enabled.
`INTEGER_ALU_SOURCE` and `FIXED_SCALE_SOURCE` select another xec.c; parent dab97a7
reproduces misaligned WORD stores in both fixtures.

`leaf64-runtime.py` compiles real modules and executes the same arithmetic and
fixed-scale matrices through native x86-64 entries, then actual C handlers. Both
WORD32 and WORD64 pass ASan/UBSan. Invalid scales additionally decline without
writes and then raise the VM exception through the protected C fallback.
ARM64/RV64 runtime modes check publication and lifetime on this host; target
instruction execution uses Unicorn. Full Inferno boots and hardware ARM64/RV64
execution remain untested. This remains a decoded-operation leaf JIT.

The core environment suite passes 33 host drivers, including the new fixtures.
