# Equal-mode cross-module calls and direct Type preflight

AMD64/ARM64 MCALL now handles calls to a different bytecode module when its
compiled flag equals the caller's. Before any write, both module pointers are
validated and their flags compared. The captured caller is saved in Frame.mr;
the callee is retained and installed in REG.M. IC remains unchanged. Builtins,
mode transitions and negative export ordinals retain C fallback. The existing
64-unit leaf capacity is unchanged. AMD64 uses short field displacements and
shared nearby failure returns to keep the broader path within that bound.

Two remaining direct descriptor-retain paths are repaired:

- linkmodule preflights the module-data table and Type before allocating the
  instance. It rejects nil/H tables and Types, dead/saturated counters, negative
  or oversized payloads, and a nil source for a nonempty clone. If the MP Type
  is Tmodlink, capacity is reserved for both header and MP retains. Empty nil
  source data keeps its previous contract. Failure consumes only the module
  reference owned by linkmod and publishes no partial instance.
- buildtokens reserves Tptr before allocating each list node. Empty output needs
  no Tptr retain. If a later word exceeds descriptor capacity, protected cleanup
  releases the complete partial list and leaves the caller's tuple unchanged.

Validation:

- mcall-native.py: 24,576 C/published RX AMD64 cases per WORD model; added
  equal-mode cross-module calls alongside self, mode-transition and builtin
  cases. Complete frame/REG/slot/refcount comparisons pass selected ASan/UBSan.
- mcall-jit.py: 9,216 complete image/write/FP checks per backend, across WORD32
  and WORD64 and entry-PC offsets 0/8. Added caller ownership, mixed and equal
  compiled flags (including full int values), preserved IC and exact stores.
- module-data-retains.py: 13 Type/table/size/source/alias cases per WORD model,
  checking zero allocation/publication on preflight failure, ownership cleanup
  and the last legal retain. All pass UBSan. MODULE_DATA_MODE selects a single
  case; successful modes 10/11 also pass ASan.
- token-retains.py: 320 cases per WORD/runtime combination (emu and os), covering
  ASCII/Rune input, empty/one/multiple-word results, descriptor saturation,
  partial cleanup and unaligned result slots. Selected ASan/UBSan checks pass.
- The previous linkmodule/buildtokens implementations fail the respective new
  drivers. Removing Tmodlink alias reservation or Tptr's limit guard fails
  independently, including signed-overflow detection under UBSan.
- All 89 --core host drivers pass. Native publication/fallback/retry/unmap
  checks pass. Source files for excluded architectures and Tk/security/crypto
  remain unchanged.

ASan limitation: the MP VM-error/rethrow path reports a stack redzone failure
inside the sanitizer's longjmp handling, after its unsupported stack-bound
warning. The same report reproduces with the previous linkmodule on its existing
allocation-error path; it is not counted as a passing ASan error-path run. The
full MP matrix is covered by UBSan and explicit allocation/refcount/image checks.
No sanitizer suppression or error-protector replacement is introduced.

| Backend | Guarded native opcodes | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 138 | 35 |
| ARM64 | 138 | 35 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

This broadens an existing opcode; counts remain unchanged out of 173 active
instructions (excluding INOP and unused IECLR). Guarded paths are partial and
are not complete native VM ABI acceptance. ARM64 instruction emulation does
not replace physical target execution.
