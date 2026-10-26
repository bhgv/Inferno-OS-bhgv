# Integer ALU and division decoded-slot checkpoint

The C interpreter and AMD64/ARM64 emitters now preflight 24 existing commands:
ADD/SUB/MUL/AND/OR/XOR for BYTE, WORD and LONG, plus DIV/MOD for each width.
Arithmetic/bit operations validate the complete result and both input spans
before reading either input. Invalid decoded slots report exBounds in C or
cause a write-free JIT decline.

Division/remainder validate and capture the divisor first. A zero divisor
reports exZdiv before consulting dividend or result slots, including when both
are null, H or unmapped address 1. This fixes the previous WORD/LONG C handlers'
premature dividend read. Once the divisor is nonzero, both remaining spans are
validated before reading the dividend. Existing signed-minimum / -1 wrap and
zero-remainder behavior, byte unsigned division and overlapping slots remain
unchanged. JIT source/result guards retain the captured divisor in private
registers until division; the adjacent shift paths retain their count priority.

Bounds checks use native pointer arithmetic and operation-specific data widths.
They cannot establish that an arbitrary nonwrapping address is mapped. No new
opcode, native ABI, private code capacity or scheduler behavior is introduced.

Validation:

* All 132 selected core drivers passed across resumed checkpoints. The compiler
  short-circuit fixture now declares native address arithmetic, H and exBounds
  for its copied division/shift handlers; all 9,240 lazy-folding/VM cases passed.

* `integer-slot-native.py`: 7,680 C slot/zero-priority/image/REG/FP cases across
  WORD32 and WORD64; NDEBUG, ASan and UBSan passed. A separate zero-only run
  checks 384 cases. The host emitted its existing setjmp/no-return stack warning.
* `integer-slot-jit.py`: 519,552 AMD64/ARM64 cases across both WORD models under
  Python -O. Independent integer oracles cover modular operations, signed and
  unsigned division/remainder, minimum/-1, truncation, all byte alignments and
  six output placements overlapping either input. Read allowlists detect early
  input access; write hooks detect even same-value writes on decline. Complete
  images/REG bytes, return state, FP modes/sticky flags and exact result spans
  are checked.
* Every short emitter capacity is rejected without publication. Exact-capacity
  bytes match sizing/output and preserve the trailing canary.
* All 72 command/backend baseline probes reproduce invalid/premature access;
  four additional C baseline probes reproduce dividend reads before zero-divisor
  errors. Sanitizer/emulator reports are classified, so compilation failure does
  not count as reproduction.
* The existing integer ALU fixture now uses explicit predicates instead of
  assertions and supplies the VM bounds error stub. Its NDEBUG/ASan/UBSan run
  passed 4,672,512 arithmetic/overlap cases across both WORD models.
* Existing ALU/divide/multiply/shift-guard leaf matrices passed 104,449
  instruction/ABI/alias/oracle cases across AMD64, ARM64 and RV64.
* The dis2c end-to-end checkpoint passed generated C compilation, malformed
  input rejection, exact image identity, loader/xec/decoder execution, all opcode
  dispatches and 144 addressing modes.
* The existing shift-slot emulator checkpoint passed 51,200 cases against the
  shared shift/division emitter code.

Both new drivers are registered in `slice-tokenize-suite.py`. The existing
`leaf64-jit.py` also accepts `LEAF64_CASE=alu` for focused arithmetic regression
without unrelated array scans. Its default sweep retains the array coverage.

Availability remains 164 guarded native paths and nine interpreter-only commands
out of 173 active commands on AMD64 and ARM64. Physical ARM64 execution and a
complete Inferno boot remain separate acceptance checks.
