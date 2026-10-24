# Integer branch alignment

`python libinterp/tests/integer-branches.py` extracts the production JMP macro,
unconditional jump and all eighteen byte/word/big comparison handlers from xec.c.
It runs both native-pointer models (WORD32 and WORD64) under UBSan by default.
Use `CFLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"`
for ASan plus UBSan. Only leak process inspection is disabled.

Each model runs 1,557,512 checks: thirteen integer boundary bit patterns,
all six predicates at three widths, independently varied source/middle/target
byte offsets 0 through 7, and unconditional jumps at all target alignments.
The numeric oracle uses unsigned byte comparisons and signed WORD/LONG values.
False branches receive an inaccessible target slot; operand bytes must remain
unchanged. Pointer targets always retain native pointer width, including WORD32.

`INTEGER_BRANCH_SOURCE=/path/to/xec.c` selects a parent implementation. The parent
217e23c fails UBSan on a misaligned Inst pointer load. Both corrected models pass
ASan/UBSan. The shared JMP change also removes the target-alignment assumption
from string comparisons and go; their remaining operand handling is unchanged.

`PYTHONPATH=/path/to/dependencies LEAF64_CASE=control python libinterp/tests/leaf64-jit.py`
checks the existing x86-64, arm64 and riscv64 native integer branch emitters with
signed-width boundary pairs and inaccessible false targets. It passes 18,822
encoder/execution/ABI/oracle checks using Unicorn and Capstone. This batch adds
no new native opcodes. Native frame/call/return JIT work remains separate;
arm64/riscv64 hardware execution and full Inferno boots were not performed.
