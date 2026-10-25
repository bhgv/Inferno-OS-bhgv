# Native subnormal real branches and Sys owner-slot fixes

## Six real branch paths on AMD64 and ARM64

BEQF, BNEF, BLTF, BLEF, BGTF and BGEF now compare non-NaN operands through
hardware floating instructions rather than ordered integer keys. Subnormal
operands remain native and observe MXCSR DAZ on AMD64 and FPCR FZ on ARM64.
Signed zeros, ordinary finite values and infinities preserve C predicates.
Floating rounding controls and sticky status are retained, including platform
input-denormal effects. FTZ/DN controls are also preserved.

Integer bit guards reject either NaN before any FP instruction changes status.
A rejected source NaN does not read the second operand. C retains its existing
quiet/signaling NaN comparison contract. A false predicate never reads the
branch target. Caller-saved FP registers are used without helper calls or a
native stack frame. Private emitter limits and all RV64 implementations remain
unchanged.

These expand six existing opcode paths: AMD64/ARM64 remain at 118 native-path
and 55 interpreter-only active commands. RV64+M remains 103/70, Zmmul 90/83,
and RV64I 87/86. This is guarded opcode availability, not a fully native block
compiler or completed 64-bit VM ABI.

## Six Sys implementation fixes

Both emu and os now publish file2chan pointer results through memmove. A native
failure still clears the result, a VM/registration exception still preserves
the old owner, and normal replacement publishes the new owner before destroying
the previous one. Existing registration/channel cleanup and error preservation
remain in C.

Dirread now reads and publishes its status/array fields separately through byte
slots. The array offset follows native WORD/pointer alignment, including padding
between a 32-bit status and native pointer. Padding is preserved. Before old
array finalization the result is already (-1,nil). All partial-array/native
entry cleanup remains active on conversion errors. The native descriptor is
captured before modifying output fields.

Fildes now captures its original WORD descriptor before publishing/clearing its
owner slot. Values outside native signed-int range report Ebadarg, leave the
result nil, and skip kdup and its release/acquire phase. Previous-owner
finalization retains its own scheduler transitions and releases that owner
exactly once. This prevents legacy WORD64 values from silently
selecting a different descriptor after narrowing. Existing native-int values,
including negative values, retain native API handling.

## Validation

- Physical AMD64 comparison against the actual production C handlers: 497,664
  cases per WORD model, 995,328 total, ASan/UBSan. Tests cover six predicates,
  signed zero/subnormal/normal/infinity/NaN representations, all four rounding
  modes, all DAZ/FTZ combinations, initial sticky flags, independent byte slot
  alignments, complete REG/data images and exact full MXCSR state. False
  predicates are repeated with an inaccessible branch target.
- Unicorn/Capstone: 25,141 ordinary real-branch checks and 130,906 FP-control
  checks across WORD models, AMD64/ARM64 and unchanged RV64 guard checks.
  ARM64 FZ/DN/rounding and FPSR input-denormal effects are checked explicitly.
- Unicorn 2.1.4 does not expose the x86 denormal-operand sticky bit for these
  comparisons. Its x86 status assertion excludes that bit; the physical AMD64
  test above checks the full register without this exclusion. No production
  behavior or sanitizer instrumentation is relaxed.
- Dirread: 9,569 cases per platform/WORD configuration, 38,276 total, ASan/UBSan,
  including all return offsets, tuple padding, old-owner finalizer observations,
  every field-allocation failure, multi-record metadata, EOF and native errors.
- File2chan: 2,209 cases per configuration, 8,836 total, complete UBSan allocation
  and registration failure matrix at all eight return offsets. ASan/UBSan normal
  paths pass 65 cases per configuration, including layout checks.
- FD ownership: 4,102 cases per configuration, 16,408 total, complete UBSan
  failure matrix. ASan/UBSan passes normal paths (1,090/configuration) and full
  files-only failures (960/configuration). The existing sandbox limitation for
  nested pipe/connection failure ASan runs remains documented in
  README.sys-fd-owners.md; no full nested-failure ASan claim is made.
- All 62 core drivers pass. Complete native AMD64 runtime matrices pass
  ASan/UBSan in both WORD models; ARM64 publication/failure/retry/freecode
  checks and generated opcode coverage pass in both WORD models.

Previous-source replays reproduce UBSan misaligned accesses in file2chan and
dirread and detect the unwanted native kdup call for an out-of-range descriptor.
Previous AMD64/ARM64 emitters reject the newly supported subnormal comparisons
in the updated branch fixture.

No Tk, crypto/security or MIPS/SPARC/PowerPC JIT files are changed. Hardware
ARM64 execution, native block/helper transitions and a coordinated native
64-bit compiler/loader/frame/GC ABI remain separate acceptance work.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/real-branch-native.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realbranch python3 libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realbranchfp python3 libinterp/tests/leaf64-jit.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-dirread.py
python3 libinterp/tests/sys-file2chan.py
python3 libinterp/tests/sys-fd-owners.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```
