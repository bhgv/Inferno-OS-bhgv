# Ordinary RET leaves and validated stack teardown

AMD64 and ARM64 now implement RET when the current frame has a real Type with
np==0, no saved module owner and non-nil/non-H caller frame and return PC. These
leaves capture every required header field before restoring FP, SP and PC. They
perform no cleanup calls, do not free an extent, and preserve the rest of REG,
frame memory and FP controls/status. The existing 64-unit staging bound remains.

Mapped frames, stack extensions, root returns and module transitions use C
fallback. A declined leaf performs no VM stores, including same-value stores.

The C return now validates captured metadata before any register publication:
invalid caller/PC markers, saved module owners, current module owners when a
module transition is needed, nil/H Type owners and negative map lengths fail
before cleanup. Extension Type metadata is read from the Stkext prefix before
restoring registers. The existing empty-message root-return exception retains
priority and leaves the current frame installed.

unextend independently rejects nil/H frame owners and nil/H/negative-map extent
Types before restoring SP/TS/EX or freeing storage. It no longer publishes the
saved stack state before discovering an invalid descriptor. Valid extension
and module-return behavior retains the existing cleanup order.

Validation:

- return-native.py: 1,344 production C / published RX AMD64 cases per WORD
  model. Includes unaligned frame headers, ordinary/mapped/extension returns,
  module restore, root returns, invalid links, descriptors and extent Types,
  and exact REG/arena/cleanup failure images. Selected ASan/UBSan runs pass.
- return-jit.py: 2,560 independent instruction/image/write/FP checks per backend
  across both WORD models and 64 frame byte offsets, including native-width
  saved links, invalid markers, map/owner fallback, malformed layouts and
  bounded publication.
- stack-lifecycle.py: 217 invalid cases per WORD model, including six added
  unextend preflight cases; selected ASan/UBSan checks pass. Replaying the old
  return and unextend implementations fails the respective new regressions.
- The exception fixture now supplies the error symbols used by actual extent
  teardown. All four emu/os and WORD configurations pass. Real stack/startup,
  native publication/fallback/retry/unmap and adjacent return tests pass.
- All 86 --core host drivers pass across the initial and resumed runs;
  the updated exception fixture passes before the resumed tail completes.
- The parallel ctypes LeafLayout repair is retained as the parent checkpoint.
  Byte-branch selection passes 15,474 checks on AMD64; character selection
  passes 6,814 checks across AMD64, ARM64 and RISC-V64, including layout probes.

| Backend | Guarded native opcodes | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 137 | 36 |
| ARM64 | 137 | 36 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

Counts cover partial guarded paths among 173 active instructions; INOP and
unused IECLR are excluded. Complete operand coverage and a complete native VM
ABI remain separate work. ARM64 is checked by instruction emulation, with
physical target execution still outstanding. No excluded architecture or
Tk/security/crypto source changes are included.
