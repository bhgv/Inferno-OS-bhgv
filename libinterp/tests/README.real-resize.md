# Real arithmetic, precision resizing and VM addresses

This series repairs thirteen VM handlers: negf, addf, subf, mulf, divf, cvtrf,
cvtfr, cvtfl, cvtfw, cvtfx, cvtxf, lea and movpc. Scalar operands are captured
with memmove before evaluation and results are published at arbitrary byte
alignment. Partial destination overlap no longer imposes C pointer alignment or
aliasing assumptions. Existing rounding, FP exceptions, integer bounds, module
errors and target pointer width are retained. Failed integer conversions and
invalid program indices leave destination bytes unchanged.

## New native leaves

ICVTRF and ICVTFR are implemented on x86-64, ARM64 and RV64 using integer
instructions only. Widening accepts signed zeros and normal finite binary32
values. Narrowing accepts signed zeros and binary64 values exactly representable
as normal binary32 numbers: exponent 897..1150 and zero low 29 fraction bits.
All other values decline without stores or FP-state changes. NaNs, infinities,
subnormals, rounding, overflow and underflow therefore retain their host C path.
No FP registers, native stack or helper calls are used; RV64I needs no M or F/D
extension. RV32 declines both opcodes. Four-byte-aligned operand slots and native
pointer widths are preserved in both VM word models.

`LEAF64_CASE=resize python libinterp/tests/leaf64-jit.py` passes 24,236 checks over
all three targets and both word sizes with Unicorn/Capstone on PYTHONPATH.
It covers raw edge patterns, random binary32 values, exact widened values and
their adjacent binary64 encodings, both overlap directions, high pointers,
inaccessible unused middle operands and inaccessible rejected destinations.
`riscv-jit.py` passes 2,070 checks, including RV32 rejection and RV64I acceptance.

## VM and host execution

`real-arithmetic.py` extracts eight actual handlers and runs 10,285,056 cases per
word model (20,570,112 total). It checks every source/middle byte alignment,
disjoint and overlapping destinations, all four rounding modes and two initial
exception states against a volatile host oracle. Result bits and exact FP flags
are compared; arithmetic NaN payload selection remains host-defined and is
checked as NaN without using floating classification instructions. Unary inputs
have an inaccessible middle operand. Native resize and negation result bits are
compared exactly, including signed zeros.

The production `leaf64-runtime.py` harness publishes a mixed module and repeats
this matrix through available native x86-64 entries, then actual C handlers.
The other FP arithmetic entries are asserted absent and retain C dispatch.
Both word models pass ASan/UBSan, including existing integer/branch/shift/lifetime
checks. ARM64/RV64 runtime modes check publication and lifecycle on this host.

`real-integers.py` now covers cvtfl, cvtfw and cvtfx at all byte alignments and
five destination placements. Fixed scales include 1, .5, -2, zero, infinity and
NaN. Boundary neighbors and 10,000 deterministic raw patterns exercise valid
results, invalid conversions and unchanged bytes on exceptions: 3,221,120 cases
per model (6,442,240 total). Both pass ASan/UBSan/float-cast-overflow checks.

`vm-addresses.py` extracts lea/movpc and checks 3,152 cases across both word
models, including overlapping pointer stores, invalid/H modules, negative and
out-of-range PCs, nil code and inaccessible operands/destinations on error paths.
Eight fresh exception processes per word model preserve VM error identities.
Both models pass ASan/UBSan.

ASan runs retain stack instrumentation; unsupported leak-process inspection is
disabled in fixtures. The environment may warn about longjmp stack-top discovery.
Parent 0aafcbd fails the new VM fixtures under UBSan on unaligned REAL, WORD and
pointer result stores. REAL_ARITH_SOURCE, XEC_SOURCE and VM_ADDRESS_SOURCE select
parent source files for these independent regressions.

The core environment suite passes 37 host drivers. Target instruction execution
for ARM64/RV64 uses Unicorn; hardware execution and full Inferno boots remain
untested. VM frames, native call/return support and GC maps are unchanged.
