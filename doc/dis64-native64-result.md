# Protected native64 C results and error-string fixes

This series adds an opt-in single-result owner guard, checks failure cleanup
through actual protection bodies from both ports, and fixes two independently
reproduced error/string defects. It does not register reference calls or assign
a shared ownership/signature wire format.

## Existing defects

The os implementation of kgerrstr duplicated the exchange operation kerrstr:
it read the caller's output buffer into the current process error string.
A read therefore changed process state. It now reads only, matching emu, and
both ports clamp the unsigned requested size to ERRMAX before the native int
copy boundary. A missing process produces the existing emu <no-up> diagnostic.
The explicit kerrstr exchange operation is unchanged.

Both kstrcpy implementations promised termination but used strncpy for truncated
spans of1..3 bytes. Those spans had no terminating zero. The short path now copies
at most span-1 bytes and writes a terminator. Nonpositive spans return before
reading either pointer. The existing longer UTF-aware ellipsis path is retained.
No whole-Unicode validity claim is added for tiny byte prefixes.

libinterp/tests/error-read.py compiles complete selected actual function bodies
against the production public emu declarations. Reads are repeatable and leave
all process error bytes intact; zero-length/null-output, tiny spans, oversized
unsigned requests, absent process and destination canaries are covered. Both
ports execute3183 checks each. Actual previous os kgerrstr and previous emu/os
kstrcpy bodies were separately compiled and reached active runtime predicate
failures, not compiler failures. The full changed emu chan.c/errstr.c objects
also compile under legacy/WORD32/WORD32+file; full OS kernel linkage is not claimed.

## Private result transaction

include/native64-result.h provides native64resultcall(slot, producer, argument)
and native64resultadopt(guard, owned). The caller supplies a live mapped owner
cell, and the producer must adopt a complete returned owner immediately, before
any yielding/collecting operation. Existing single-owner ptradd allocation must
not collect a still-unpinned value. Partial trees before adoption remain the
producer's responsibility; destructors must not throw.

The mutable guard lives outside the actual runtime kprotect/setjmp frame.
Adoption records the owned value before attempting ptradd. Its rooted flag is
set only after successful pin allocation. Consequently a pin allocation error
still has an owner to release, without removing some other alias's existing pin.

On producer decline or VM error, the guard reads and saves the current error,
clears its own value/flag, removes only its successful pin, releases its value,
restores the original error and returns zero. The old caller owner remains
published and reachable. The caller can then use existing krethrow.

On success, the new value is published before the old owner is destroyed. The
guard detaches and removes its pin first, so destruction callbacks see a mapped
new owner and no zero-reference temporary root. An H result and replacement by
the same object are supported when the producer owns the additional reference.
Invalid owner-cell preflight and a missing producer return zero without invoking
it. This is trusted C consistency/ownership code, not an arbitrary-pointer verifier.

## Executed scope

native64-result.py reuses the complete actual collector body and actual heap.c
objects from the previous gate. It replaces the old test-only direct kprotect
with the complete selected actual kprotect/krethrow bodies from emu and os.
Actual kstrcpy/kgerrstr bodies are used too. Public process/environment routing
and a host setjmp/longjmp error-label boundary are supplied by the fixture;
architecture-specific error-label assembly and full scheduler unwind are not run.
The formatter implements only the ordinary C formats used by this fixture.

Nine producer behaviors cover normal publication, error before allocation,
decline/error after pin, actual pin-metadata allocation failure, same-object
replacement, failed pin beside an independent alias pin, H and refused nil.
Every mode runs across8 process states,3 initial colors and direct/rethrow paths.
Both WORD32 selections and both protection ports give1728 transactions,
960 actual delegated destructor checkpoints and212952 active predicates/run.
The callback deliberately changes the error string during actual destruction;
the producer error must survive. Actual collector epochs check temporary/caller
roots, actual reference counts balance, and external pin tables/error depths
return to their expected state. Metadata malloc failure targets actual ptradd;
it is not simulated by a callback returning an error code.

Fresh owning Limbo O1 and O3/NDEBUG strict UBSan runs of native64-layout.py pass
with this driver included. The direct error-string suite also passes O3
ASan+UBSan. For protected nonlocal execution, optimized ASan reports an ignored
no-return stack operation with missing stack bounds, followed by a stack-shadow
failure in rootset's local Frame copy. That ordinary optimized ASan run is NOT
accepted. An O3 ASan+UBSan run with -fno-inline -fno-omit-frame-pointer passes
all1728 cases with address/UB instrumentation enabled; host symbolization and
no-return stack warnings remain. No sanitizer checks are suppressed except the
pre-existing unavailable LSan process scan. This is not warning-free host or
whole-runtime sanitizer certification.

The layout suite invokes this driver with its exact freshly built compiler.
error-read.py is registered in CORE. The selected source closure needs both
port chan.c files, emu/port/errstr.c and os/port/proc.c in addition to the new
header/fixtures; vendors must import those owning sources explicitly.

Reference structural signatures, actual reference-returning builtin admission,
throwing-destructor policy, arbitrary nested partial trees, header-temp roots,
full VM unwind/scheduler integration, parallel collectors and physical target
execution remain separate. Default Runtab/NC64,43-op np0 pilot and general
inspector false are unchanged; complex C implementations remain C.
