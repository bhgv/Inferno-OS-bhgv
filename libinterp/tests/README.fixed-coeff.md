# Frame coefficients, exception publication and string representations

The shared `fixedword` reader checks nil/H frame identities before forming any
frame-word address. Eight C handlers now use it: MULX, DIVX, MULX0, DIVX0, CVTXX0,
MULX1, DIVX1 and CVTXX1. Reads remain byte-safe at every alignment. CVTXX uses
only decoded operands and does not acquire a frame dependency.

The x86-64, arm64 and RV64 leaf backends compile MULX0, DIVX0 and CVTXX0. A private
`fixedcoef` field supplies the coefficient byte offset, alongside the existing
frame and scale offsets. Compiler adapters derive these from the current VM
layout; REG, Frame, Module and the Dis wire format are unchanged.

All parameters and operands are captured before result publication, including
results overlapping coefficient or scale slots. Arithmetic matches the C VM:
modular 64-bit products, signed shifts from -63 through 63, truncating signed
division and narrowing to the active WORD size. Divisor zero and -1 decline to C
before publication, retaining VM errors and minimum/-1 handling. Invalid scales
also decline: C's zero-numerator shortcuts still determine whether an invalid
scale is observable. RV64 requires division, plus multiplication for MULX0 and
DIVX0; a Zmmul-only build publishes none of the three entries. Native leaves make
no FP calls or FP-state changes and use no native stack or allocation.

String fixes cover unsigned byte versus Rune comparison, invalid owner/INT_MIN
length rejection before comparison, and splitting empty H owners. `splitc` reads
and publishes through byte-safe slots, preserving owners if allocation fails.
`iraise` captures an unaligned owner, validates tuple descriptor size and prepares
the actual string message before retaining/publishing the exception value. Nil
owners, malformed descriptors and failed UTF-8 cache allocation preserve the
previous exception state and reference counts.

## Checks

```
python libinterp/tests/fixed-coeff.py
COEFFICIENT_NATIVE=1 python libinterp/tests/fixed-coeff.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=coefficient python libinterp/tests/leaf64-jit.py
python libinterp/tests/fixed-ops.py
python libinterp/tests/raise-slots.py
python libinterp/tests/string-slots.py
python libinterp/tests/string-branches.py
python libinterp/tests/slice-tokenize-suite.py --core
```

Set `CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'` for
the sanitizer matrices. Native AMD64 execution requires Linux x86-64. Every
fixture extracts actual production bodies; WORD32 fixtures retain host pointers.

* Coefficients: 1,097,744 C checks per word model, and 2,195,472 C/native checks
  per model with `COEFFICIENT_NATIVE=1` (4,390,944 total). Independent unsigned
  product/division-magnitude oracles check slot/frame alignment, exact/partial
  operand and coefficient aliases, native fallback and eight nil-frame errors.
* Three-backend coefficient emission/execution: 30,082 checks. Capstone decodes
  the code and Unicorn executes WORD32/WORD64 edge/random values, signed scales,
  divide guards, inaccessible results and frame-slot aliases. RV64 frame/operand
  slots retain their existing four-byte minimum alignment contract.
* The complete nine-operation fixed driver adds 1,625,400 sanitizer cases and
  37,800 independent Python arbitrary-precision checks per word model.
* Message preparation: 192 cases per word model, including both string forms,
  tuple layouts, nil/H owners, cache allocation failure and preserved REG/refs.
* String slots: 79,271 cases per word model, including shared/empty splits and
  failed allocations. String branches/comparisons: 438,276 per model, including
  the complete 256x256 mixed byte/Rune ordering oracle and invalid representations.

`FIXED_COEFFICIENT_SOURCE`, `RAISE_SLOT_SOURCE` and `STRING_SOURCE` can select the
parent sources. They reproduce a nil-frame crash, exception publication before
message-allocation failure and the empty-owner/unaligned split failure. Hardware
arm64/RV64 execution and complete Inferno boot remain separate acceptance work.
