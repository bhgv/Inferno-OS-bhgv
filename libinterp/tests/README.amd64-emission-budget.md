# Bounded AMD64 late fixups

The private emitter counts candidate bytes after its 512-byte buffer fills so
that the final publication check can decline oversized code. `x64byte` already
bounded ordinary emission, but late branch patches wrote directly to the array.
A valid string-store layout with an existing 500-byte prefix reproduced UBSan's
`index 675 out of bounds for type uchar [512]` at the old absolute-index fixup.
This is a private staging-boundary defect; the current ordinary publication
matrix below does not claim an executable-memory overflow.

All late AMD64 byte fixups now use `x64patchbyte`, which checks the staged byte
index before storing. It preserves requested-size accounting, emitted instruction
selection, LeafMax, caller capacity, runtime ABIs and the final zero-publication
rule. Exhausted candidates do not bypass the writer during size measurement.

Run `python3 libinterp/tests/amd64-emission-budget.py`. The C fixture compiles the
actual header at O1 and O3 with UBSan. Each build checks 7,712 near-capacity
prefix/helper/publication combinations, WORD32/NC64, shifted metadata, independent
leading/trailing canaries and every earlier prefix byte. Caller capacities 0–128
check that declines and tails leave destination bytes untouched.

The publication-only matrix (`-DBUDGET_PUBLICATION_ONLY`) has 3,096 cases. Parent
06e33807 and the fix both produce fingerprint `3f48522272abbdc3`, including return
extents and destination bytes. The parent full fixture reproduces the sanitizer
failure; the fixed full fixture passes. Native `leaf64-runtime.py` also passes
RX publication, fallback tables, failure/retry/unmap, 2,336,256 integer ALU checks,
1,170,432 fixed-scale checks and 40,960 scalar transfer checks on AMD64 WORD64.
This does not claim physical ARM64/RV64 execution.
