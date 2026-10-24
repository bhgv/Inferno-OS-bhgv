# Exact integer-to-real JIT leaves

ICVTWF and ICVTLF now have decoded-operation leaves on x86-64, ARM64 and RV64.
The accepted domain is signed integers with magnitude strictly less than 2^53.
Every accepted integer is represented exactly in binary64 under all rounding
modes. The leaf computes unsigned magnitude, rejects values outside the domain,
normalizes the bits in at most 52 shifts, and combines exponent, fraction and
sign. Zero becomes positive zero. No floating registers or instructions, helper
calls or native stack are used. Rejected values return zero without writes or
changes to floating-point state, leaving rounding and inexact exceptions to C.
Some exactly representable larger integers also take this conservative fallback.
RV64I suffices; RV32 declines both operations. WORD32 still writes an eight-byte
REAL and uses native-width pointers.

The production C handlers capture potentially unaligned source values before
conversion and publish results with memmove. Current host rounding and exception
behavior is retained, including inexact conversions outside the native domain.

`python libinterp/tests/integer-real.py` extracts actual production handlers and
runs 1,843,200 checks per WORD model. It covers positive/negative power-of-two
boundaries, both sides of the 2^53 guard, integer extrema and deterministic random
bit patterns. All source byte alignments and disjoint/identical/partly overlapping
destinations are tested. Four rounding modes and two initial exception states
are compared against separate volatile host WORD/LONG oracles. Whole-buffer,
PC and operand-register effects are checked. The unused middle operand is
inaccessible. Both models pass with
`CFLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"`.
Only unsupported leak-process inspection is disabled; stack instrumentation stays
active. `INTEGER_REAL_SOURCE` selects another xec.c; parent 85d1896 reproduces
an unaligned REAL result store under UBSan.

`LEAF64_CASE=integerreal python libinterp/tests/leaf64-jit.py` passes 11,892
encoder/execution/ABI/oracle checks over all three targets and both word sizes.
Unicorn/Capstone must be on PYTHONPATH. Inputs use high pointers, four-byte slot
alignment, aliases in both directions and an inaccessible unused operand.
Rejected inputs also run with an inaccessible destination. The RISC-V driver
passes 2,060 checks including no-M acceptance on RV64 and rejection on RV32.

`leaf64-runtime.py` publishes real modules and repeats the full new matrix through
native x86-64 entries, then actual C handlers, with exact result bits and FP flags
checked. Both WORD models pass ASan/UBSan alongside the existing arithmetic,
shift/division and fixed-scale matrices. This series also repairs the extracted
shift/division harness's missing SHIFT_DIV_FIRST default introduced by its parent.

The core environment suite passes 35 host drivers. ARM64/RV64 compiler publication
and lifetime tests run on the host, while target instruction execution uses
Unicorn. Hardware ARM64/RV64 execution and full Inferno boots remain untested.
This remains a decoded-operation leaf JIT with unchanged VM frames and GC maps.
