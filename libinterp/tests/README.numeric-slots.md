# Nine numeric commands: slot preflight and full normal CVTFX power scales

Based on experiment `e14f0343575fa6a1c9d24893e54af2872f28fa5e`.

## Runtime and native changes

ADDF, SUBF, MULF, DIVF, CVTXF, CVTFX, EXPW, EXPL and EXPF now bound their used
decoded operand spans in the interpreter and AMD64/ARM64 leaves. Null, H and
wrapping slot spans fail before operand access or publication. Both source slots
fit before either value is read. Real operations also bound the destination before
floating conversions or arithmetic can modify sticky exception state. WORD-sized
exponents, WORD-sized fixed inputs/results and eight-byte real/big values retain
their distinct widths; pointers remain native width in both WORD models.

Integer powers preserve the zero-base/negative-exponent error before inspecting
the result slot. Other inputs bound the result before modular multiplication or
publication. The negative path never negates a signed minimum exponent. Existing
signed/unsigned rules, alias captures and FP squaring/reciprocal order are kept.
Native failures return zero with unchanged bytes, registers and FP state; the C
handler supplies bounds/zero/invalid errors. Span arithmetic is not a proof that
an arbitrary address belongs to a mapped allocation.

CVTFX now accepts every normal exact signed binary64 power scale from 2^-1022
through 2^1023, instead of only 2^-16 through 2^16. The fractional-bit test still
rejects non-powers; biased exponents 1..2046 reject zero, subnormal scales, NaN
and infinity. Non-unit scales additionally require normal source and exact normal
product exponents. The old signed rounding, symmetric output bounds and host FP
arithmetic remain authoritative. Unit scales retain their zero/subnormal behavior.
Unsupported products and result boundaries keep C; no FP operation occurs before
all decline guards pass. No native helper ABI, opcode ID or buffer limit changes.

## Validation

```
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 -O libinterp/tests/numeric-slot-native.py
python3 -O libinterp/tests/numeric-slot-jit.py
LEAF64_CASE=realinteger python3 libinterp/tests/leaf64-jit.py
LEAF64_CASE=realarithmetic python3 libinterp/tests/leaf64-jit.py
LEAF64_CASE=power python3 libinterp/tests/leaf64-jit.py
LEAF64_CASE=realpower python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/leaf64-runtime.py
python3 libinterp/tests/real-to-integer-native.py
python3 limbo/tests/fold-effects.py
python3 limbo/tests/fold-casts.py
python3 libinterp/tests/jit-coverage.py
```

The new C/RX fixture executes 265,648 cases in WORD32 and 266,096 in WORD64
under NDEBUG/ASan/UBSan. Every normal scale exponent is tested with both source
and scale signs, four rounding modes, seeded FP flags and four result placements.
Invalid spans cover every byte alignment, poisoned sibling operands, zero-error
priority and complete memory/REG/FP preservation. Separate volatile C arithmetic
provides the rounding/exception oracle. Only unsupported LeakSanitizer process
enumeration is disabled; stack/address/undefined instrumentation remains active.
The sandbox emits its known executable-name/longjmp notification warnings.

The instruction-emulation fixture executes 71,040 cases per architecture across
both WORD widths. Independent Python arithmetic and whole-image oracles cover the
expanded domain, modular/negative powers, scalar arithmetic, malformed slots and
no stores/FP effects on decline. Every short output capacity, exact sizing/image
and a trailing canary are checked. Existing real/integer/native/VM/compiler fold
matrices remain active; their minimal extraction fixtures now declare the bounds
identity and native pointer/sentinel types used by the production guards.

NUMERIC_SLOT_SOURCE, NUMERIC_SLOT_EMITTER_DIR, NUMERIC_SLOT_OP and
NUMERIC_SLOT_ARCHES allow explicit parent substitution. All 27 command/backend
probes reproduce the previous invalid memory accesses. The two new drivers are
registered in slice-tokenize-suite.py. Fifteen adjacent numeric/VM/compiler drivers
pass, including 22,413,312 real-to-integer cases per WORD model. The complete
WORD32 native module/publication fixture also passes ASan/UBSan. The broader
201-driver selection is not claimed as a full rerun for this numeric series. Physical ARM64/RV64 execution and full
Inferno/compiler boots are separate from host and instruction-emulation checks.

## Availability

These are corrected and broadened existing native paths. All profiles retain
173 active Dis commands (175 slots minus invalid INOP and unused IECLR).

| Backend | Has native path | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 164 | 9 |
| ARM64 | 164 | 9 |
| RV64 + M | 103 | 70 |
| RV64 + Zmmul | 90 | 83 |
| RV64I | 87 | 86 |

Availability includes guarded/partial paths and is not full semantic or block-JIT
coverage. Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources are unchanged.
