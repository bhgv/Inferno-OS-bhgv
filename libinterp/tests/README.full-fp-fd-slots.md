# Full operand coverage for four conversions and FD publication fixes

## Native FP conversion coverage

AMD64 and ARM64 no longer restrict ICVTWF/ICVTLF to integer magnitudes below
2^53. Signed native conversion now accepts all WORD/LONG bit patterns, including
both signed extremes, and produces the current rounding mode's binary64 result
with its actual sticky exception effects.

ICVTRF/ICVTFR now perform native binary32/binary64 conversions for every input
representation. This includes subnormals, inexact narrowing, overflow, infinities
and NaNs. NaN payload selection remains platform-defined. Both emitters capture
the input before writing an overlapping destination, use caller-saved registers,
and inherit the host C FP environment without changing its controls. The native
paths are substantially shorter than the previous exact-bit normalization code.

These are operand-coverage expansions of four existing opcode paths. They add no
opcode IDs. RV64 retains its existing guarded integer-only bit implementations;
no RV floating extension or ABI requirement is introduced. Complex operations
continue to use optimized C helpers.

## FD ownership, alignment and validation

Sys.open, create and fildes in both emu and os now copy the previous owner from
an unaligned VM return slot, publish H before running its finalizer, and publish
a replacement only after its constructor succeeds. Previously the finalizer
could observe a stale live pointer in the result slot. A parent-source replay
fails the new finalizer observation assertion even with an aligned slot.

Sys.pipe in both environments now validates nil arrays and absent/wrapping data
spans before creating native descriptors. It copies pointer-sized array elements
without requiring native alignment. Both new ends are installed before either
old end is finalized; the WORD status is also copied into an unaligned slot.
The allocation-failure cleanup and all-or-nothing owner replacement are retained.
No generated frame, pointer layout, descriptor ownership policy, Tk, crypto/
security component or excluded legacy JIT is changed.

## Verification and platform boundaries

- Complete AMD64 runtime matrices pass with ASan/UBSan for WORD32 and WORD64.
  Each includes 1,843,200 integer-to-real checks and 10,285,056 real arithmetic/
  resizing checks, covering all four rounding modes, preexisting flags, full
  memory effects, unaligned slots and partial aliases.
- Unicorn/Capstone: 65,096 resizing and 33,364 integer-to-real instruction/ABI/
  alias/oracle checks pass across the two priority emitters and unchanged RV64
  guarded paths. ARM64 checks the standard FPSR flags and preserves FPCR.
- The FP oracle models the target's underflow rule: default ARM FPCR detects
  tininess before rounding, whereas the x86 SSE host oracle detects it after.
  Thus a value just below binary32's smallest normal number can round to that
  normal value while still setting ARM underflow. This is an architectural
  difference, not an ignored test failure. The adjustment requires an inexact
  conversion of a nonzero input smaller than 2^-126. See the official
  [Arm floating-point guide](https://documentation-service.arm.com/static/61718b68ac265639eac5b6bc?token=).
- FD ownership matrix: 2,702 cases per emu/os and WORD32/WORD64 configuration,
  10,808 total, passes UBSan and exact guard/resource checks. It covers every
  byte offset, shared old owners, native failures, allocation failures and
  malformed pipe descriptors. Both old result absence and complete new pipe
  publication are checked inside finalization.
- ASan/UBSan passes the 698 normal-path cases and 288 file allocation-failure
  cases per configuration. The full nested pipe/connection failure matrix is
  not claimed to pass ASan here: the existing sandbox nonlocal-jump stack-bound
  limitation described in README.sys-fd-owners.md remains. A full ASan run still
  reproduces that stack-bound report; UBSan full-failure coverage is retained.
- All 60 core host drivers pass. The publication fixture now explicitly includes
  these four conversion IDs; ARM64 RX publication/failure/retry/freecode checks
  pass for both WORD models, and the RV64 fixture passes.

Physical ARM64 execution and full native 64-bit compiler/loader/frame/GC ABI
acceptance remain separate work. Emulated IEEE tests do not replace hardware
validation or tests of every nondefault FP control option.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/leaf64-runtime.py
LEAF_WORD_BITS=32 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/leaf64-runtime.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=resize python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=integerreal python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/sys-fd-owners.py
```

Coverage remains 118 native opcode paths / 55 interpreter-only commands for
AMD64 and ARM64, out of 173 active Dis instructions. Other paths can still be
guarded or partial. RV64+M remains 103/70, RV64+Zmmul 90/83 and RV64I 87/86.
