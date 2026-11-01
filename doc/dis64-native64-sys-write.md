# Explicit native64 print/fprint C gateways

When native64 Sys declarations and DIS_NATIVE64_FORMATTER are explicitly enabled,
runt.c exports Sys_print64 and Sys_fprint64. Native64SysWriteRequest is declared
in native64-sys-call.h. Its format request contains the rooted pattern, immutable
typed tail, process, capacities and scalar resolver; fd is an actual caller-rooted
Sys_FD for fprint and is ignored by print. The generated packed Sys_print/fprint
entry refusals and registration/file admission guards remain unchanged.

The implementation checks generated F_Sys_print/F_Sys_fprint geometry/maps.
The fprint fixed body roots both the FD and String through a complete managed
argument frame. Its checker requires exact persistent port TFD identity, not an
arbitrary structurally similar heap object. H denotes descriptor -1; nil or wrong
managed FD types are refused before allocation. TFD remains port-owned through
the call. print writes to descriptor1. Native kwrite handles unavailable/negative
descriptors; format validation precedes native descriptor failures.

Output growth finishes under the VM before any write. A first resolver call
checks a current writable four-byte WORD cell and may allocate/collect, cancel
with nil or raise; failure at this point produces no native output. The gateway
then releases the VM, issues exactly one kwrite, and reacquires it. Native bytes
and managed FD/argument roots remain owned throughout. Release/acquire are trusted
nonthrowing scheduler transitions; kwrite may return or raise. Mutable ownership
and the successful-release flag live outside kprotect's jump frame. On native
error the outer cleanup reacquires before retiring buffers/arguments. Native or
unwind diagnostics survive a scheduler boundary that alters the current error.

After reacquisition the resolver runs again and must return the current logical
scalar cell; no pre-write address is reused. A short write, zero or -1 is published
as the exact WORD result. Counts below -1 or greater than the requested bytes are
refused instead of narrowing invalid results. There are no automatic write retries.
No allocating callback intervenes between final resolution and four-byte commit.
The resolver owes live mapped-cell provenance/serialization; bounds checks cannot
authenticate arbitrary pointers or repair an invalid caller root contract.

Output is irreversible: a post-write cancellation/resolver error or invalid
native count leaves the old scalar result intact, but does not undo already
written bytes. Return status therefore reports transaction publication separately
from the native count stored only on successful completion. Callers must not infer
zero output from a false status or blindly retry a failed post-write transaction.
This is a synchronous trusted C contract, not a new wire variadic instruction.

## Active tests

```
python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/fresh/limbo
```

Per emu/os and WORD32/WORD32-file combination,1,200 output transactions span three
collector phases, eight process states, print/fprint and25 side-effect/failure
modes. They invoke actual exported runt.c wrappers, complete owning header bodies,
actual Type/heap/collector and protected-port errors. Only scheduler/native output
services are bounded fixtures; this is not a complete scheduler or device test.
Tests count release/write/acquire, capture exact output bytes, force collection
inside native output and both resolutions, move a logical scalar destination,
and retire the caller's FD root while its copied fprint argument must retain it.

Full/short/zero/-1 writes, native throw, impossible native counts, pre/post-write
cancel/throw, exhausted capacity, constructor/native allocation failures, nil/wrong/
H FD, missing persistent TFD, invalid canonical values, invalid result spans and
embedded NUL are covered. Scalar publication must change exactly four bytes and
leave the neighboring word intact. Native error text must survive intentionally
changed reacquisition diagnostics. All native/managed/Type owners balance.
Two additional compiled removal controls must fail: missing unwind reacquisition
and an otherwise-valid fprint map with only the format root but no FD root. Existing
kind, buffer-retirement and return-Type/element controls remain active.

Whole runt.c compiles with all four gateway exports for NDK ARM64/x64 and for
riscv64-linux-android35 using shared Android64 C declarations. This is compile
coverage, not full boot, Android/RISC-V support or target execution. Existing ASan
injected-unwind/optimized UTF findings remain open and are not suppressed here.
