# Wider native real-to-integer ranges and directory/path result slots

## Three expanded AMD64/ARM64 conversion paths

CVTFW, CVTFL and CVTFX now accept a wider symmetric source range before their
existing half-unit correction and truncating conversion:

| Result width | Previous absolute bound | New strict absolute bound |
| --- | --- | --- |
| 32 bits | 2^30 | 2147483647.5 |
| 64 bits | 2^62 | 2^63-1024, the binary64 value immediately below 2^63 |

The bounds are checked as integer representations before any floating effects
or destination writes. The largest accepted binary64 input below either bound
can still receive its signed half-unit correction in every rounding mode
without leaving the destination range. At the 64-bit boundary, an upward
correction of the excluded bound itself could reach 2^63; it correctly remains
on the C path. This is deliberately a safe symmetric subset, not a claim that
all individually valid extreme inputs are native.

CVTFX retains exact +1/-1 scale guards and chooses the correction sign from the
actual scaled result. Non-finite/out-of-range inputs and unsupported scales
retain the C handler with unchanged FP state on leaf rejection. Caller-saved
registers, destination aliasing, the private 64-unit capacity and RV64 behavior
are unchanged. ARM64 materializes the full shifted binary64 bound rather than
assuming its lower bits are zero.

No opcode IDs are added: AMD64/ARM64 retain 118 native-path / 55 interpreter-only
active commands; RV64+M 103/70, RV64+Zmmul 90/83 and RV64I 87/86. These count
guarded decoded-operation paths, not a completed native block JIT.

## Eight Sys wrappers and shared string/directory helpers

Both emu and os now publish stat/fstat status and directory data without typed
accesses through possibly unaligned tuple pointers. The record offset is derived
from each actual generated return type, including its explicit _pad4 field;
a guessed native {WORD,Sys_Dir} layout is incorrect in legacy WORD64.

Directory conversion publishes each new string directly into its VM owner slot
before another allocation. It then copies scalar metadata through an aligned
Sys_Dir value. This preserves the previous incremental replacement behavior:
a conversion failure keeps status -1, already replaced strings stay owned by
the result, remaining old strings stay owned, and scalar fields retain their
previous values. Record/tuple padding is preserved. There is no disconnected
native string staging area hidden from VM ownership during allocations.

Fd2path captures its native descriptor, clears its unaligned string owner slot
before finalization, and retains/frees the native path across every string
conversion failure. Export stages its native scalar result before copying to
an unaligned return slot. Existing scheduler/native call order is preserved.
Security/authentication functions are not modified.

Retstr and retnstr already used memmove internally; their declarations and
definitions now accept void* byte slots rather than promising String**
alignment. Direct pointer arguments and the native calling convention remain
unchanged. Both retain the previous owner until conversion/publication succeeds.

## Validation

- Physical AMD64 C/native differential: 5,505,024 checks per WORD model,
  11,010,048 total, ASan/UBSan. New edges exercise both sides of both bounds and
  the largest accepted inputs, all four rounding modes, sticky exceptions,
  signed scales, unaligned slots, partial source/destination/scale aliases and
  side-effect-free rejection. Every accepted conversion is compared with the
  actual production C handler.
- Unicorn/Capstone conversion matrix: 54,586 instruction/alias/FP-state checks,
  including ARM64 full constant materialization and boundary cases in every
  rounding mode. Existing documented Unicorn x86 conversion precision-flag
  limitations remain covered by the physical native differential above.
- New stat/fstat/fd2path fixture: 816 cases per platform/WORD configuration,
  3,264 total, ASan/UBSan. It uses generated frame types and their explicit
  offsets, real heap/string constructors, all eight byte alignments, tuple and
  record padding, shared previous string owners, every partial conversion
  failure, native failures, nil string fields and independent native-buffer
  accounting. Old fd2path finalization observes an already cleared owner slot.
- Expanded scalar status matrix: 5,280 cases per configuration, 21,120 total,
  ASan/UBSan, now including export status/flag-mask/scheduler behavior.
- Existing dirread regression: 9,569 cases per configuration, 38,276 total,
  ASan/UBSan, protecting the other shared directory-conversion caller.
- Existing full FD ownership UBSan matrix passes in all four configurations.
- Complete native AMD64 runtime matrices pass ASan/UBSan in both WORD models;
  ARM64 publication/failure/retry/freecode and generated opcode coverage pass.
- All 63 core drivers pass, including the new production-function driver.

Replays of the preceding emu source reproduce separate UBSan failures in stat,
fstat, fd2path and export. SYS_STAT_SOURCE selects the earlier source;
SYS_STAT_OPERATION=0/1/2 isolates the query, and SYS_STAT_ALIGNED selects aligned
slots. SYS_STATUS_OPERATION=15 isolates export in the scalar fixture.

No Tk, security/crypto or MIPS/SPARC/PowerPC JIT source is changed. Hardware
ARM64 validation, native blocks/helper transitions and a coordinated native
64-bit compiler/loader/frame/GC ABI remain separate acceptance work.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/real-to-integer-native.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realinteger python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/sys-stat-slots.py
python3 libinterp/tests/sys-status-slots.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-dirread.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```
