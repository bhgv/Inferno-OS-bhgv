# ASCII concatenation and wide decimal checkpoint

## Instruction availability

`python3 libinterp/tests/jit-coverage.py` counts 173 active Dis instructions,
excluding INOP and the unused IECLR enum slot. WORD32/native-pointer64 and
legacy WORD64 currently give the same availability counts.

| Processor/profile | Guarded native paths | Interpreter-only remaining |
| --- | ---: | ---: |
| x64 / AMD64 | 133 | 40 |
| ARM64 / AArch64 | 133 | 40 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 base integer | 87 | 86 |

ADDC adds one instruction to AMD64 and ARM64 availability. Guards still decline
unsupported operands to the C interpreter. This is partial operand coverage,
not completion of the full native 64-bit VM/frame/GC ABI. Other backend sources
remain unchanged.

## ADDC without allocation

The destination must be the same operand slot as the left source. Both string
owners must be present ASCII representations; the left owner must be unique
and both native-width reference counts must be nonzero. The right input is
bounded by 128 bytes. Signed capacities validate both inputs and bound the full
64-bit sum before forming the destination address or publishing any bytes.

The leaf copies forward and then stores the new logical length. Self append,
empty input, byte values above 127 and embedded NULs are supported. Forward-safe
overlap is accepted; overlap requiring reverse copying declines before any
write. Rune conversion, allocation, shared destinations, language-nil inputs
and large sources retain C fallback. No helper, ownership, cache, PC or FP
operation is introduced. The 64-unit staging limit is retained. The AMD64
short-displacement adapter checks its private field offsets before emission.

The C concatenator now rejects a dead left or right heap owner before copying
or returning through a nil shortcut. Its growth-overflow check now applies only
when replacement storage is actually needed. Previously a valid large ASCII
string could reject appending an empty allocated string even though its existing
capacity was sufficient. Actual growth and ASCII-to-Rune promotion still reject
the impossible replacement allocation before scanning or allocating anything.

The large-string regression reserves a genuine virtual mapping above the old
growth threshold and touches only its headers. It does not give a tiny object
an out-of-bounds logical length. Native and C paths must retain the original
owner and perform no VM heap allocation. The mapping is released after testing.

## Decimal parsing in 64 bits

CVTCL and legacy-WORD64 CVTCW now accept at most 18 digits, plus an optional
sign. The complete accumulator, multiply, addition, negation and result move
are 64-bit operations. At most 18 digits cannot overflow signed 64-bit arithmetic.
WORD32 CVTCW keeps its nine-digit limit so host-long width cannot change the
language result. CVTCF accepts at most 15 digits, all exactly representable in
binary64 in every rounding mode, and retains negative zero.

Longer text, whitespace, decimal points, exponents, Rune storage and malformed
metadata retain write-free fallback. The existing terminator guard preserves
the C UTF-view behavior. No new Dis instruction or public ABI layout is needed
for this expanded coverage.

## Verification

All 77 core host drivers pass.

```sh
python3 libinterp/tests/slice-tokenize-suite.py --core
python3 libinterp/tests/string-append-native.py
python3 libinterp/tests/string-append-jit.py
python3 libinterp/tests/decimal-native.py
python3 libinterp/tests/decimal-jit.py
DECIMAL_VM_PARSER=1 DECIMAL_ACCEPTED_ONLY=1 python3 libinterp/tests/decimal-native.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/string-append-native.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/decimal-native.py
```

The native concatenation fixture passes 29,952 C/RX-JIT cases per WORD model
under UBSan and ASan+UBSan, checking whole heap and operand images, aliases,
ownership, allocation counts, exact VM errors and the large virtual mapping.
Removing either owner check or restoring the old growth condition independently
fails the new regression. The emulation fixture passes 20,134 ARM64 and 20,136
AMD64 instruction/image/write/FP checks. It includes operand slots overwritten
by appended bytes, both overlap directions and reference counts above 2^32.

The wider decimal native fixture passes 811,008 actual C/JIT cases per WORD
model under UBSan and ASan+UBSan. The expanded AMD64/ARM64 instruction matrix
passes 844,000 combined checks with all FP rounding modes and clear/preexisting
exception flags. The optional production-parser fixture compiles the actual
libmath/dtoa.c under an independent symbol and passes 703,488 C/JIT cases per
model for accepted native real inputs, alongside the existing integer matrix.
The default fixture retains libc as an independent general-parser oracle.

Selected ASan runs retain stack instrumentation and still emit this sandbox's
existing nonlocal-jump runtime warning. They are not a claim that all VM error
paths pass ASan. The new native driver is registered in the core suite; the new
instruction-emulation driver is registered in its optional JIT group. Physical
ARM64 execution and a complete VM boot remain separate acceptance work.
