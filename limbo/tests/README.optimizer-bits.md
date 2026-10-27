# Optimizer bit-set regressions

Run from the repository root:

```sh
python3 limbo/tests/optimizer-bits.py
```

The fixture extracts production bit-set declarations, masks and helper bodies
from optim.c. Host ulong32 and ulong64 models each pass 16,415,642 membership,
range, count, low/high-position, diagnostic-print and Boolean truth-table checks
under UBSan with immediate failure. The models retain native pointers; they do
not represent target execution or a full compiler build.

Storage cells are explicitly u32int, matching the existing five-bit cell index
and 32-bit bit-count/search algorithms. Unsigned masks include bit 31 and full
cell ranges without signed-shift overflow. Allocation rounds a requested bit
count using quotient/remainder rather than a potentially overflowing addition.
Negative range ends retain their clear-to-end meaning; empty and reversed
ranges do not touch storage. The printer shifts unsigned cell masks.

Tests cover sizes 0 through 257, every single-bit set/clear, every endpoint pair
within 96 bits, all 16 Boolean operations with random cells and destination
aliasing, and diagnostic output. Byte-array and Boolean oracles are independent
of the production mask calculations. The original LP64 implementation fails
cell-count checks; its 32-bit configuration also fails UBSan on sign-bit masks.
No bytecode layouts, target word sizes, or JIT interfaces change.

The fixture also passes ASan+UBSan with `ASAN_OPTIONS=detect_leaks=0`; the sandbox
cannot support LeakSanitizer process inspection. Every fixture allocation is
explicitly freed. Full optimized Limbo program compilation remains outside this
helper-level fixture.


Latest graph traversal and wide bitmap/aggregate counters:
[README.optimizer-graphs.md](README.optimizer-graphs.md). The bitmap fixture now
uses explicit predicates and a leak-inspection hook, retaining its full oracle
under NDEBUG and ASan/UBSan without relying on assertions.
