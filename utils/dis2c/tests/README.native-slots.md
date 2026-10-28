# Native-safe Dis-to-C decoding and scalar fast paths

The AOT translator had retained typed operand dereferences and unchecked native
pointer arithmetic after the interpreter and priority JIT paths had moved to
checked, alignment-independent access. Generated MOVB/MOVW/MOVL/MOVF and twelve
WORD/LONG ADD/SUB/MUL/AND/OR/XOR operations could therefore execute undefined
unaligned C loads/stores or bypass the VM's decoded-slot errors.

This series fixes the generated code while retaining specialization:

- Direct and indirect addresses use the same inline `decaddress` and `decindirect`
  helpers as `dec.c`. Signed offset overflow, nil/H bases, complete indirect
  pointer cells and second-offset overflow have the same VM error order.
- Immediate slots use byte offsets into the original instruction, preserving
  loader-patched native branch targets without unaligned `Inst` member access.
- Four scalar moves check both complete slots and use overlap-safe `memmove`.
  REAL moves copy raw bits, including NaN payloads, without evaluating them.
- Twelve modular integer operations check all three slots before any data read,
  capture both inputs through `memmove`, compute in UWORD/ULONG and publish only
  the final width. Partial result/source/middle overlaps remain valid.
- A generated step reads only the required module/link header fields through
  byte copies, validates instruction-count identity and the entire program span,
  and advances the original Dis PC with checked native address arithmetic.
  Missing/wrapping headers and invalid PCs reject before decoding/publication.
- Complex commands still call their real VM handlers. Scheduler accounting,
  module ownership, image matching, signatures and exception PCs are unchanged.

Generated files include the internal `dec-addr.h` from the matching runtime;
rebuild/regenerate them together. This is an AOT correctness change, not a new
native JIT opcode, native VM ABI definition or full 64-bit boot result.

## Verification

```
python3 utils/dis2c/tests/native-slots.py
python3 -O utils/dis2c/tests/test.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
```

`make -C utils/dis2c test` now includes both translator drivers. The new fixture
compiles the actual translator, generates a real Dis image/C module, and links
its callable registered step with real native VM headers, `dec.c` and all 16
production scalar handlers. WORD32 and WORD64 pass 162,656 explicit predicates
under ASan/UBSan and NDEBUG:

- 9,216 operand-alignment/partial-alias cases per WORD model compare all REG
  bytes and the entire operand image with real interpreter execution.
- All 144 legal source/destination/middle mode combinations run at each of eight
  instruction and eight frame alignments, including unaligned indirect cells.
- All 173 active opcode selections are compared; complex operations use observed
  callbacks here rather than simulated scheduler/GC/channel side effects.
- Sentinel/wrapping slots reject before otherwise poisoned peers are read.
- Decoder error identities and partial register publication match the real VM.
- Packed native module/link headers and invalid headers/counts/PCs are checked.

The decoder-error matrix runs the two routes in separate forked processes;
error callbacks save shared snapshots and exit directly, avoiding this sandbox's
ASan shadow-cleanup failure when repeatedly unwinding instrumented decoder frames.
No production or fixture stack instrumentation is disabled. Unavailable
process-based leak inspection is disabled using a small test-only helper.
ASan symbolization/no-return warnings may still appear.

Five independent parent-generator replays fail at runtime: `slots`, `addresses`,
`invalid`, `decode` and `headers`. The aligned opcode/fallback family passes on
the parent. Reproduce with `DIS2C_NATIVE_SOURCE=/path/to/parent-dis2c.c`,
`DIS2C_NATIVE_CASE=<family>` and `DIS2C_WORD_BITS=32`; parent controls use UBSan.
The parent source is a464a95's `utils/dis2c/dis2c.c`. No fixture compilation failure
is counted as a reproduced production defect.

The original integration fixture is repaired for current loader dependencies:
`dissignedword`, `exModule` and `exNilref`. Its assertions are replaced with
explicit predicates, including inherited test adapters. It passes with C NDEBUG
and Python -O while executing the actual loader, decoder, `xec`, C attachment,
44-step loop/quanta, exact-image rejection, malformed inputs and all opcode/mode
routes. Production loading/signature policy is not modified by this series.

Fresh priority-JIT checks retain 173/173 complete entries and 166/173 guarded
native fast paths (95.95%) on AMD64/ARM64. Native AMD64 dispatch tests retain
177,152 routes, 6,183 compiler cases and 295 integration checks per model; ARM64
compiler preflight retains 6,215 per model. Physical ARM64 and complete Inferno
boot remain unverified. Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources
are unchanged.
