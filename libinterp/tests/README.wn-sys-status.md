# Conditional WN mask guards and further Sys result fixes

## AMD64 and ARM64

MULX1, DIVX1 and CVTXX1 now select the rounding-mask condition before checking
whether the exponent is negative. WN and VN+WN can consequently execute natively
at exponents 0 through 63 when the captured original signs require no mask.
When a mask is needed, the existing negative-exponent guard still returns to C,
which preserves exInval rather than allowing an invalid host shift. Exponents
outside -63 through 63 and zero divisors retain the existing fallback.

VN polarity and zero-operand behavior are unchanged. Neither backend grows its
private buffer or emitted code size: this change moves the existing sign test
and conditional transfer ahead of the scale guard. There are no helper calls,
stack allocations, GC writes or changes to RV64.

These are extensions of three existing opcode paths, not new opcode IDs.
AMD64/ARM64 retain 118 native-path and 55 interpreter-only active opcodes.
RV64+M retains 103/70, RV64+Zmmul 90/83, and RV64I 87/86. Counts describe
availability of guarded decoded-operation leaves, not a completed block JIT.

## Eighteen Sys implementations

Both emu and os now publish millisec, wstat, fwstat, sleep, print, fprint and
werrstr scalar results with memmove rather than dereferencing potentially
byte-unaligned WORD pointers. Native results are staged at VM WORD width.
Directory packing/freeing, formatting buffer ownership, native API ordering and
release/acquire behavior remain in the shared C implementations.

Emulator sleep now defines a zero result for every representable nonpositive
period; previously it left the result slot unchanged. Positive emulated sleep
preserves the host result, while kernel sleep preserves its zero result.
Both implementations validate periods before narrowing to the native scheduler
interface. Mount and bind similarly reject flags outside native signed-int
range. Rejection reports Ebadarg and -1 before any release or native call.
This prevents a legacy WORD64 argument from silently becoming a different
32-bit period or flag set. Existing signed-int arguments retain their handling.

Print now returns -1 when stdout is absent instead of leaving its return slot
unchanged. Successful/failed writes preserve the native return value. Werrstr
consumes the supplied string before publishing its zero result.

## Regression coverage

The status fixture extracts all 15 real production wrappers and generated frame
types. It uses controlled clocks, native calls, formatting buffers and scheduler
adapters, without real sleeps, file updates or namespace operations. Tests check
all eight return alignments, full surrounding-byte guards, native-width bounds,
short/allocated formatting, missing stdout, invalid fprint descriptors, native
errors and exact release/acquire/call counts. The kernel clock expression is
adapted to a controlled clock, not used to test tick conversion itself. Error
unwinding from a real blocking sleep is outside this fixture.

- Sys matrix: 5,240 cases per platform/WORD model, 20,960 total, ASan/UBSan.
- Actual C versus executable AMD64 coefficient leaves: 4,390,928 baseline,
  518,400 VN/VN+WN, and 162,000 WN-only cases per WORD model, ASan/UBSan.
- Unicorn/Capstone: 137,284 general rounding and 63,412 VN edge cases across
  AMD64, ARM64 and the unchanged RV64 guards.
- Complete AMD64 runtime matrices in WORD32 and WORD64, ASan/UBSan; ARM64
  publication/failure/retry/freecode checks in both WORD models.
- Core VM/compiler regression suite and generated opcode coverage checkpoint.

Replaying the preceding inferno.c separately reproduces four defects: UBSan on
an unaligned wstat result, unchanged sleep(0) output, unchanged closed-stdout
print output, and loss of high mount flag bits. Aligned isolated replays use
SYS_STATUS_ALIGNED=1 with SYS_STATUS_OPERATION=11/12/5; sleep and closed stdout
also select SYS_STATUS_ZERO_SLEEP=1 or SYS_STATUS_PRINT_MODE=2 respectively.
SYS_STATUS_SOURCE selects the earlier source file. No sanitizer checks are
removed; the existing hook disables only unavailable leak-process inspection.

No Tk, security/crypto or MIPS/SPARC/PowerPC JIT sources are modified. ARM64
hardware execution, native blocks/helper transitions and the complete 64-bit
compiler/loader/frame/GC ABI remain separate acceptance work.

```
COEFFICIENT_NATIVE=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/fixed-coeff.py
COEFFICIENT_NATIVE=1 FIXED_COEFFICIENT_CASE=wn CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/fixed-coeff.py
python3 libinterp/tests/sys-status-slots.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=roundedcoefficient python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=vncoefficient python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```
