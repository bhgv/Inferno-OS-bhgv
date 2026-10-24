# Scalar transfer alignment and overlap

`python libinterp/tests/scalar-transfers.py` extracts ten production VM handlers:
movb/movw/movl/movf and cvtbw/cvtwb/cvtws/cvtsw/cvtwl/cvtlw. It checks 40,960
cases for each of WORD32 and WORD64 with native-width pointers. Source offsets
cover all byte alignments; destination offsets cover disjoint, identical and
both directions of partial overlap. Sixteen raw patterns exercise sign boundaries,
truncation, extension and NaN payloads. Whole-buffer comparison catches writes
outside the destination; PC and operand registers must remain unchanged.

The default is UBSan. Both models also pass with
`CFLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"`.
Only leak process inspection is disabled. `SCALAR_TRANSFER_SOURCE` selects another
xec.c: the parent 5677197 reproduces a misaligned WORD store under UBSan.

`LEAF64_CASE=scalar python libinterp/tests/leaf64-jit.py`, with Unicorn and Capstone
on PYTHONPATH, runs 4,404 encoder/execution/ABI/oracle checks over all three
64-bit emitters and both word models. It includes high pointers, four-byte slot
alignment, both overlap directions, exact aliases and an inaccessible unused
middle operand. RV64 memory hooks reject naturally misaligned emitted accesses.

The production compiler harness `leaf64-runtime.py` also publishes and executes
all ten native x86-64 leaves, comparing both native and actual C handlers against
the scalar oracle at every byte alignment. Its 40,960 scalar cases per word model
pass ASan/UBSan alongside existing RX publication, fallback and lifetime checks.
ARM64/RV64 modes test compiler publication/lifetime on the host; their instruction
execution uses Unicorn. No new native opcodes or hardware target boot claims are
made by this series.
