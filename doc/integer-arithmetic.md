# Integer arithmetic and JIT contract

The active VM uses byte=8, big=64 and WORD=sizeof(long)*8. WORD is 32 on
existing ILP32 targets and remains 64 in the legacy LP64 configuration.
This change does not enable the proposed Dis64 ABI or make ordinary Dis32
modules compatible with LP64. In particular it does not import InferNode's
32-bit-int-in-64-bit-slot layout.

Integer add/subtract/multiply wrap modulo the operand width. Unsigned C
arithmetic and explicit bit-pattern-to-signed conversion avoid signed overflow
and out-of-range unsigned-to-signed casts. Signed division truncates toward
zero; the remainder has the dividend's sign. A zero divisor raises exZdiv
before storing a result. MIN/-1 wraps to MIN, and MIN%-1 is zero.
Byte division/remainder are unsigned.

Shift counts must be nonnegative and less than the operand width, as required
by doc/limbo/limbo.ms. Invalid counts raise "invalid shift" before storing.
Left shifts zero-fill and truncate to width; byte and logical right shifts
zero-fill; word/big arithmetic right shifts sign-fill. No C signed shifts or
out-of-range shift counts are evaluated. This deliberately replaces historical
host-dependent behavior for out-of-contract counts, rather than adopting
InferNode's count & 63 rule.

ARM32 and x86 byte/word shifts use the interpreter helpers for now. x86 integer
division/remainder also use helpers: signed IDIV mishandled both MIN/-1 and
unsigned byte quotients above 127. This is a performance tradeoff until guarded
native sequences have execution coverage. Existing big shifts/division already
use helpers. Native x86/ARM add/subtract/multiply retain their wrapping results.

RISC-V's leaf JIT retains native valid-count shifts and wrapping arithmetic.
Invalid shifts return 0 without a store, so xec invokes the validating helper;
division/remainder already use helpers. The existing encoder tests cover the
RV32/word32, RV64/word32 and legacy RV64/word64 profiles. No x64 or ARM64 backend
is added, and the older MIPS/Power/SPARC backends are outside this change.

## Verification

```
python3 libinterp/tests/integer-arithmetic.py
PYTHONPATH=/path/to/dependencies python3 libinterp/tests/riscv-jit.py
```

The integer test extracts production helper/opcode bodies and compares them
with an independent Python integer oracle in WORD32/WORD64 configurations.
It runs 77,778 cases, including boundaries, deterministic random operands,
all three destination placements (distinct, source alias, middle alias),
zero division, invalid counts and no mutation on errors. UBSan is enabled by
default; CC and CFLAGS can override the compiler/options. These are host tests
with a stubbed VM error boundary, not an emulator boot or target ABI test.

All 18 libinterp Python suites pass with Unicorn/Capstone/Keystone installed.
Full xec.c and comp-386.c pass host syntax checks with Linux/386 headers;
comp-arm.c passes with Linux/arm headers. These checks do not replace target
builds or on-device execution, and no performance measurements were made.

Reference snapshots reviewed before implementation:
- infernode-os/infernode: abe17e727f642b90bf16c764932a2d447b9eb94b
- caerwynj/inferno64: 1ee772fc34b2a46689609f8cede01865929067c3
