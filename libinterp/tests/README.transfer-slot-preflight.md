# Scalar transfer and conversion slot checkpoint

The C interpreter and AMD64/ARM64 emitters now preflight native source/result
spans for 18 existing commands:

* MOVB, MOVW, MOVL and MOVF.
* CVTBW, CVTWB, CVTWS, CVTSW, CVTWL and CVTLW.
* LEA and NEGF.
* CVTRF, CVTFR, CVTWF, CVTLF, CVTFL and CVTFW.

Null/sentinel slots and exclusive-end address wrap are rejected before loading
values or changing the floating-point environment. Result checks come first, so
an invalid result never triggers a source read, even if the source would fault
or contains a signaling NaN. The C path reports exBounds; declined JIT paths
preserve all REG/image bytes and FP control/sticky flags.

LEA checks only its writable WORD slot: its source is an address value, including
null, H, low identities and high native pointers. The C implementation explicitly
converts through uintptr before narrowing to WORD. The unused middle operand is
never accessed by any command in this series. Input/output scalar widths remain
independent of native pointer width and may differ from each other.

Successful transfers retain overlap semantics and existing FP rounding/range
behavior. The separate scaled CVTFX path is unchanged. Arithmetic span checks do
not prove that arbitrary nonwrapping addresses belong to mapped allocations.

Validation:

* All 130 selected host core drivers passed across resumed checkpoints. The
  fixed-zero-div compiler fixture now supplies the native address/sentinel/error
  declarations required by the actual MOVW handler; its 28,800 lowering/VM/alias
  cases passed.
* The incoming d899383 shift series was preserved by rebasing. Its new core
  driver passed 2,944 C cases and its JIT driver 51,200 cases; integer-ops and
  shift-div regressions passed again. Both new transfer fixtures passed again
  against the merged emitters/interpreter. All 131 currently selected core
  drivers are covered across the base checkpoint and incoming-change reruns.

* `transfer-slot-native.py`: 16,016 C preflight/FP/LEA cases across WORD32 and
  WORD64, with exact VM errors, full memory/REG preservation and all four FP
  rounding modes. Passed NDEBUG, ASan and UBSan. The host emitted its existing
  no-return stack warning around the setjmp fixture.
* `transfer-slot-jit.py`: 183,072 cases across AMD64/ARM64 and both WORD models,
  run under Python -O. Read allowlists detect premature input reads; write hooks
  check both write-free declines and the exact successful output span. Oracles
  cover unaligned and overlapping inputs, sign/truncation boundaries, NaN payload
  sign changes, ignored LEA addresses, exact FP conversions and all rounding
  modes with seeded sticky flags.
* Every insufficient emitter capacity is write-free; exact-capacity output
  matches sizing/publication and preserves its trailing canary.
* All 18 C and 36 per-backend old-version probes reproduce invalid/premature
  memory access. Each failure is classified from the sanitizer/emulator report,
  rather than accepting compilation failures as a reproduction.
* Five existing scalar/FP C/RX oracle fixtures now use explicit predicates
  instead of assertions. Their NDEBUG + ASan/UBSan runs cover 81,920 scalar,
  3,686,400 integer-to-real, 20,570,112 real arithmetic/resize, 6,442,240 rounded
  cast and 35,389,440 native rounded-cast cases across both WORD models.
* The existing scalar/address/resize/integerreal/realinteger leaf matrices passed
  225,990 instruction/ABI/alias/oracle cases across AMD64, ARM64 and RV64.
* The incoming raw-copy fixture capacity fix passed all 411,264 existing
  AMD64/ARM64/RV64 copy/overlap/span/FP cases under Python -O.

Both new drivers are registered in `slice-tokenize-suite.py`. Opcode availability
remains 164 native paths and nine interpreter-only commands out of 173 active
commands on AMD64 and ARM64. Paths remain guarded/partial; physical ARM64 and a
complete Inferno boot remain separate acceptance checks.
