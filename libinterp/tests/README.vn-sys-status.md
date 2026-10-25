# Native VN rounding and Sys scalar return slots

## Three expanded native rounding paths

AMD64 and ARM64 now support VN and VN+WN in MULX1, DIVX1 and CVTXX1.
VN-only accepts scales -63 through 63; combined VN+WN, like WN-only, currently
accepts right shifts -63 through -1. Existing frame, scale and zero-divisor
fallbacks remain. RV64 nonzero-flag handling is unchanged.

The VN correction uses captured original operand signs. Multiplication and
conversion form coefficient-minus-one at WORD width, optionally negate it at
that width, sign-extend it, and add it after shifting but before division.
Division adds its signed unit correction after division. Zero operands retain
the interpreter's zero shortcut, so they cannot acquire a spurious VN unit.
For MULX1/CVTXX1, VN reverses WN mask polarity; DIVX1 keeps its original numerator
sign condition. Product overflow cannot change correction selection. All
intermediate additions use the existing modular 64-bit contract.

Both backends use shared emission helpers and caller-saved registers without
runtime helper calls or native stack frames. The fixed private 64-unit limit is
unchanged. WORD32/WORD64 MUL/DIV/CVT rounded leaves occupy 61/57/56 and 60/57/55
four-byte units on AMD64, and 60/56/55 and 55/52/51 units on ARM64 respectively.
These are operand-mode expansions of three existing opcode IDs.

## Sixteen Sys implementation fixes

Sys.dup, remove, seek, unmount, chdir, mount, bind and iounit now stage their
native result and copy it into the VM return slot in both emu and os. The return
can have any byte alignment; seek still returns a complete 64-bit LONG in both
WORD models. Native call order and scheduler release/acquire boundaries remain.

Sys.dup descriptor arguments and Sys.seek's start argument are checked before
narrowing to a native signed 32-bit int. Legacy WORD64 arguments outside that
range now return -1 with Ebadarg before release or a native call, rather than
silently selecting another descriptor or seek mode by losing high bits. Values
already representable by native int, including existing negative sentinels, keep
their native handling. Seek's full-width offset is not narrowed.

The new fixture extracts production functions and uses actual generated frames.
Controlled native API adapters check arguments, offsets, result width, string
selection, scheduler state and call count without performing namespace or file
operations. It tests all eight result offsets with surrounding-byte guards.
Previous-source replays fail UBSan on an unaligned WORD store and, with aligned
slots, fail the wide-argument/status oracle.

## Validation

- Actual C/native VN and VN+WN differential: 518,400 cases per WORD model,
  1,036,800 total, pass ASan/UBSan. It covers zero/sign shortcuts, minimum/maximum
  operands, coefficient wrap at WORD width, all six output/frame aliases and
  eight operand/frame alignments. The default fixed-coeff driver now also runs
  this family, while explicit `wn` and `vn` selectors remain available.
- Existing native fixed coefficient baseline: 4,390,928 per model, 8,781,856 total;
  WN-only regression: 162,000 per model, 324,000 total; both pass ASan/UBSan.
- Unicorn/Capstone: 137,284 general rounding checks and 63,412 VN edge checks
  pass, including ARM64 WORD32 correction wrap and unchanged RV64 rejection.
- Sys status fixture: 3,840 cases per emu/os and WORD32/WORD64 configuration,
  15,360 total, pass ASan/UBSan, including early-error/no-native-call behavior.
- Both complete native AMD64 runtime matrices pass ASan/UBSan. All 61 core
  drivers pass. ARM64 RX publication/failure/retry/freecode tests pass in both
  WORD models; the RV64 publication fixture passes.

No Tk, crypto/security component or excluded legacy JIT is modified. Physical
ARM64 execution and the complete native 64-bit compiler/loader/frame/GC ABI
remain separate acceptance work. These decoded-operation leaves do not claim
native basic-block compilation or complete coverage of every rounding mode.

```
COEFFICIENT_NATIVE=1 FIXED_COEFFICIENT_CASE=vn CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/fixed-coeff.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=vncoefficient python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=roundedcoefficient python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/sys-status-slots.py
python3 libinterp/tests/jit-coverage.py
```

Availability remains 118 native opcode paths / 55 interpreter-only instructions
for AMD64 and ARM64, out of 173 active Dis instructions. Counts still include
partial paths. RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I 87/86.
