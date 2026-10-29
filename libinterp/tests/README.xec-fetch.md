# Checked VM fetch, immutable opcode selection and scheduler completion

The normal `xec` loop is shared by interpreted Dis, decoded leaf JIT entries and
registered AOT steps. It previously checked only that a PC was non-null and did
not wrap by `sizeof(Inst)`. It could fetch outside the current Module program,
read a different opcode after the decoder overwrote aliased REG storage, decrement
zero/negative callback budgets, and interpret a callback-selected native-PC
context as bytecode on the following iteration.

This series changes the actual common executor:

- Validate the scheduler's Prog prefix and positive entry quantum before copying
  registers. Pending kill messages are consumed before dereferencing VM owners.
- Read link/module metadata through byte copies after complete native-prefix
  checks, including packed headers and Jit tables.
- Validate the entire bytecode program extent, positive count and relative Inst
  alignment/membership of every next PC before reading an opcode or entering AOT.
  A packed program base remains supported; absolute native alignment is not
  imposed on Inst images.
- Capture both opcode and addressing byte before the decoder publishes REG.
  Advance from the captured original PC, preserving the decoded instruction's
  identity even if its bytes alias REG.s/d/m and change during decoding.
- Reject inactive opcodes and missing/sentinel decoder/C/JIT/entry callbacks at
  their appropriate boundary. A successful fast path still needs no C handler;
  a successful AOT step still needs no decoder or C-table entry.
- Capture JIT entry pointers before calling synchronization/operation callbacks.
  Complete C tails can release their Module/Jit without later executor reads of
  those source owners. Ordinary callback effects and bytecode PCs are retained.
- Recheck execution mode on every iteration. A C/AOT/JIT callback that switches
  to native-PC mode enters `comvec` with the remaining quantum, instead of fetching
  its native PC token as a Dis instruction.
- Zero/negative callback budgets yield with saved IC=0; INT_MIN is never
  decremented. Positive budgets retain the existing one-step decrement behavior.

The global live REG still records intermediate publication on VM errors, while
Prog.R is saved only after normal completion, as before. Bounds checks establish
numeric extents and expected layout, not mapping ownership of arbitrary pointers.
No opcode, frame, Dis image, native-PC layout or module ABI is changed.

The subsequent [instrumented-loop series](README.instrument-loops.md) shares
these checks with debugger, coverage-profiler and benchmark executors in emu/os.
The measurements below describe the normal `xec` route.

## Verification

```
python3 libinterp/tests/xec-fetch-native.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 utils/dis2c/tests/test.py
python3 libinterp/tests/riscv-runtime.py
python3 utils/tests/native-bootstrap.py
```

The new driver borrows the actual executor, AMD64 compiler, complete RX tails,
C executor and native VM headers, and separately links real `dec.c` under a
private test symbol. WORD32 and WORD64 pass 485,124 parent-process predicates
under ASan/UBSan and NDEBUG. Child error predicates also execute but are not
included in that counter. No fixture or production stack instrumentation is
disabled; unavailable process-based leak inspection is disabled. Sandbox
symbolization/no-return warnings may appear.

The positive matrix executes all 173 active commands under six controlled
C/fast/complete/AOT routing profiles, eight packed-PC offsets and four quantum
sizes. Complex-command callbacks observe routing rather than simulate their
scheduler/GC/channel side effects. Separate tests execute real RX ADDW fast
paths with nil/H C handlers, real DIVW fast refusal and ALT complete C tails.
Successful AOT callbacks are tested with unusable decoder/C-table entries to
verify that unused routes are not consulted.

Real decoder writes reproduce the REG/instruction alias: the source pointer
field initially contains an IRUNT instruction, and decoding its AFP source
changes the instruction's opcode byte to IADDW. C, native IRUNT and declined-AOT
routes must still execute the captured IRUNT. This is not a mock decoder rewrite.

Additional matrices cover packed link/module/cache fields, callback IC values
0/-1/INT_MIN/1, pending kills, invalid scheduler/module/program pointers, inactive
opcodes, missing callback slots, and running off the last instruction. Falloff
runs include actual ADDW fast code and actual C-only ALT tails. C/AOT/fast/real-tail
switches to a native-PC token must enter only the native callback with the exact
remaining quantum and callback-selected MP.

Malformed routes run in fresh child processes through the ordinary VM error
adapter so abandoned instrumented frames are not reused across error cases.
Existing native JIT/C integration still tests source-owner release inside a real
complete tail. This is not a full scheduler/OS/signals/GC integration or an Inferno
boot test.

Twelve independent parent-source families fail during execution: `yield`,
`transitions`, `alias`, `packed`, `bounds`, `module`, `opcodes`, `budget`, `process`,
`kill`, `tables`, `falloff`. `routes` and `native` pass on the parent. Replay with
`BRIDGE_XEC_SOURCE=/path/to/parent-xec.c`, `BRIDGE_WORD_BITS=32`, `FETCH_CASE=<name>`
and UBSan CFLAGS. The parent is 8ab265a's `libinterp/xec.c`.

Focused actual-JIT controls also fail independently: `FETCH_CASE=yield
FETCH_ROUTE=6` uses an RX ALT tail; `FETCH_CASE=alias FETCH_ROUTE=1` uses RX IRUNT;
`FETCH_CASE=falloff FETCH_ROUTE=1|2` uses RX ADDW or an RX ALT tail. These are
execution failures, not fixture compilation failures.

Existing adapters add the new VM error identities and real instruction counts;
RV64 JIT source is unchanged. The normal native JIT fixture retains 177,152
routes, 6,183 compiler cases and 295 integration checks per WORD model. ARM64
compiler preflight retains 6,215 per model. AOT loader/decoder/xec integration,
the shared RV64 runtime adapter and native bootstrap (including all 17 Limbo
units) pass.

Fresh readiness remains 173/173 complete JIT+C entries, with 166/173 guarded native
fast paths (95.95%) on AMD64 and ARM64. This series strengthens execution of those
entries rather than adding opcodes. Physical ARM64 execution and complete 64-bit
VM boots remain unverified. Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources
are unchanged.
