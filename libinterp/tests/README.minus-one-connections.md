# Native minus-one division and unaligned connection results

## Division operand coverage

AMD64 ordinary signed DIV/MOD no longer rejects -1. A shared emitter handles
that divisor using modular negation and a zero remainder, bypassing the x86
IDIV MIN/-1 overflow trap. Other nonzero divisors retain the existing IDIV path.
The helper is also used by fixed frame/coefficient operations, whose intermediate
is always 64 bits even with WORD32 output. Captured operands remain intact until
the possibly unaligned/overlapping result store.

ARM64 fixed frame/coefficient leaves now accept -1 using SDIV's defined modular
overflow result. Ordinary ARM64 division already supported it. Zero divisors,
invalid frames/scales and unsupported rounding flags still defer to C without
publishing a destination. Frame-pointer H checks remain distinct from the -1
arithmetic operand checks. RV64 guards and all private buffer limits are unchanged.

This extends existing opcode paths; it adds no new opcode IDs and does not
replace complex C helpers with native call stubs.

## Connection result alignment and ownership

Sys.dial, announce and listen in both emu and os now copy their WORD status into
any byte-aligned VM result slot. A helper derives the connection payload offset
from the actual generated Sys.dial return declaration, using aligned scratch
storage rather than dereferencing an unaligned tuple. No tuple ABI is hardcoded
or changed; existing padding is untouched.

The clearconnection and makeconnection helpers now consume raw slot addresses.
They capture previous owners with memmove and publish the complete empty/new
connection before finalizing its old handles and directory. Allocation failures
still preserve the previous connection, while native-call failures clear it.
Cleanup retains the original error string and closes each new descriptor once.
The listen input may continue to borrow the old connection's directory until
native work and replacement construction finish.

The expanded fixture places every connection result at offsets zero through
seven, preserves surrounding/padding bytes, and observes published state during
old-owner destruction. Replaying the previous source reproduces UBSan misaligned
member access in the connection result. No Tk, crypto/security component or
excluded legacy JIT is changed.

## Validation

- Fixed coefficient native differential: 4,390,928 cases per WORD model,
  8,781,856 total, with ASan/UBSan. WN rounding adds 162,000 per model, 324,000
  total. Both matrices now accept -1 and compare actual interpreter behavior,
  destination aliases, alignment, complete memory effects and error fallback.
- The complete native AMD64 runtime matrices pass with ASan/UBSan for WORD32
  and WORD64, including ordinary division/remainder and fixed frame matrices.
- Unicorn/Capstone: 28,798 ordinary division, 81,426 fixed frame, 30,364 fixed
  coefficient and 137,284 rounded coefficient checks pass, 277,872 total. The
  ordinary division selector includes byte/WORD/LONG, MIN/-1, zero divisors,
  random signed operands and six destination alias arrangements.
- FD owners: 3,430 cases per emu/os and WORD32/WORD64 configuration, 13,720 total,
  pass UBSan with exact resource/guard checks. ASan/UBSan passes all 866 normal
  cases and 288 file allocation-failure cases per configuration. The existing
  sandbox ASan limitation for nested pipe/connection failure jumps remains;
  full-failure ASan acceptance is not claimed here.
- All 60 core host drivers pass. ARM64 compiler RX publication/failure/retry/
  freecode tests pass in both WORD models; the RV64 publication fixture passes.

Physical ARM64 execution and a complete native 64-bit compiler/loader/frame/GC
ABI remain separate acceptance work. Instruction emulation does not replace
hardware testing.

```
PYTHONPATH=../jit-test-deps LEAF64_CASE=divide python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=framefixed python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=coefficient python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=roundedcoefficient python3 libinterp/tests/leaf64-jit.py
COEFFICIENT_NATIVE=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/fixed-coeff.py
python3 libinterp/tests/sys-fd-owners.py
```

Availability remains 118 native opcode paths / 55 interpreter-only commands
for AMD64 and ARM64, out of 173 active Dis instructions. Some paths still have
other guards. RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I 87/86.
