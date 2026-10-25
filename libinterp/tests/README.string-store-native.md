# Existing-character native stores and validated string capacity

AMD64 and ARM64 now implement INSC through LeafStringStore for an existing
character of a uniquely owned string. ASCII stores accept the C interpreter's
32-bit Rune conversion when its result is below Runeself. Rune strings accept
all 32-bit Rune bit patterns. Index comparisons retain the full VM WORD width;
a high 64-bit index cannot wrap into a valid character position.

The leaf captures value, index and owner before any mutation. It rejects C-null
and language nil owners, nonunique native-width reference counts, INT_MIN
representation lengths, invalid logical capacity and out-of-range indexes.
There is exactly one byte or four-byte character store on success, with no
allocation, helper, reference/color/cache update, length change, PC/frame write
or FP instruction. Append, representation promotion and shared-string copying
remain in C. Both emitters retain the existing 64-unit publication limit and
use caller-saved registers only.

The private LeafLayout gains stringmax, populated from the actual String.max
field by comp-leaf64.h. It does not change String, Heap, REG or Dis layouts.
The ctypes adapter is updated without shifting any existing field. RISC-V
uses its independent layout and retains its previous native opcode counts.

The production C insc now rejects a zero-reference owner and a length exceeding
logical capacity before writing characters, allocating replacement storage or
releasing an owner. string2c validates ASCII capacity before writing the NUL
terminator and Rune capacity before reading payload or refreshing its cache.
The historical oversized-Rune byte-count error remains exNomem; capacity
inconsistency is exHeap. Nil and empty valid strings retain their behavior.
CVTCW/CVTCL leaves validate the same capacity before parsing. Their AMD64 nil
success branch now uses a checked staged 32-bit displacement rather than
assuming the expanded path fits a short branch.

Validation:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/string-store-native.py
python3 libinterp/tests/string-store-jit.py
STRING_CAPACITY_ONLY=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/string-codecs.py
python3 libinterp/tests/decimal-jit.py
python3 libinterp/tests/decimal-native.py
python3 libinterp/tests/slice-tokenize-suite.py --core
python3 libinterp/tests/jit-coverage.py
```

The actual AMD64 compiler fixture executes 196608 C/JIT cases per WORD model
under ASan/UBSan. It varies all three operand alignments, ASCII/Rune storage,
full-word values, source/payload aliases, boundaries, dead/shared/saturated
references, invalid capacity and all four FP rounding modes with existing
exception flags. It compares complete heap/operand/REG images, preserves
preexisting UTF caches byte-for-byte and requires no VM heap allocation.
C fallbacks that allocate are covered by the existing production codec suite;
the new differential fixture executes successful in-place C stores and exact
error cases, and checks native decline for allocation paths.

The independent Unicorn matrix passes 262388 actual AMD64/ARM64 instruction,
layout/capacity/owner/alias/FP checks with VM storage above 4 GiB. A write hook
requires exactly the intended character store on success and no store on
fallback. The expanded decimal matrix passes 488924 checks, including negative
or insufficient String.max and malformed private max-field offsets.

Capacity error tests preserve complete allocation images including guard bytes,
allocation counts and existing Rune UTF cache pointers/content. Replaying the
parent C string2c with a forged length 1000 in a four-character allocation
reproduces a one-byte heap-buffer-overflow under ASan. Replaying parent insc
with STRING_STORE_PROFILE=9 fails the expected zero-reference error boundary.
STRING_SOURCE selects a previous C implementation for these replays.

The heap-query and raise fixtures construct strings manually and now initialize
real logical capacity and supply exHeap. Their production diagnostic/exception
operations remain unchanged. The core suite contains 76 host drivers; the
publication fixture contains 78 Dis instructions and checks failed allocation,
protection, retries, entry presence and executable-map destruction on both
WORD models. Native AMD64 execution and ARM64/RV64 publication remain distinct
from physical ARM64/RV64 execution and complete VM/OS acceptance. The previously
documented full ASan string-return/nonlocal-jump limitation is not resolved by
this series.

Active opcode coverage is now AMD64 131 native-path / 42 interpreter-only and
ARM64 131/42. RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I 87/86. These
are guarded native paths among 173 active instructions, not full semantic
coverage or a completed native 64-bit VM/frame/GC ABI port.
