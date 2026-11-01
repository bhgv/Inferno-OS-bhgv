# Explicit native64 Sys formatting gateways

With DIS_NATIVE64_SYS_HEADER and DIS_NATIVE64_FORMATTER enabled, runt.c exports
Sys_sprint64 and Sys_aprint64. Their trusted C request contract is declared in
native64-sys-call.h; the owning implementation is in native64-sys-format.h.
These take an explicit typed tail and late result resolver, not a packed frame.
They do not replace Sys_sprint/Sys_aprint in generated registration tables and
are not a file variadic ABI. The four existing native64 packed-entry refusals,
Sys registration guard and default file admission remain unchanged.

The gateway uses actual generated F_Sys_sprint/F_Sys_aprint geometry and map8.
It verifies their one-String fixed prefix and builds its own fixed schema and
canonical/live checkers. Callers no longer supply an arbitrary fixed prefix,
checker callback or target producer. A synchronous control snapshot retains the
process, pattern, capacities and resolver while the existing argument constructor
copies/owns the tail and pins the managed frame. Source pattern/values/descriptors
remain readable, immutable and caller-rooted through construction. Type identity
checks require trusted real managed provenance; integer bounds/alignment do not
authenticate arbitrary pointers.

The producer reads the copied format prefix and calls the existing bounded
formatter/conversion owner. String uses c2string, bytes use mem2array. The resolver
is invoked only after successful production, may collect, cancel with nil or
raise, and must return the current caller-owned mapped result cell. Immediately
before publication the gateway checks the old result's exact Tstring identity,
or Tarray with Tbyte elements for aprint. No callback allocation occurs between
this identity check and the existing guarded commit. H is an accepted old owner;
nil and incompatible old identities fail while preserving the destination.
The caller serializes that cell and the producer must not change it. Contexts
and callbacks must not escape or asynchronously reenter this synchronous call.

Requests require a valid borrowed Prog, resolver, bounded initial/limit geometry,
canonical contiguous tail slots and exact value span. Rejected geometry, canonical
values and fixed identities never allocate or enter the producer. Later errors
retire acquired managed/native owners and preserve the original diagnostic.
Successful output is published only after formatting, conversion, pinning and
late destination validation. Complex conversion/format logic remains C.

## Tests

```
python3 utils/tests/native64-arguments.py --formatter --limbo /path/to/fresh/limbo
```

For each emu/os and WORD32/WORD32-file combination, 1,104 gateway transactions
span three collector phases, eight process states, String/array results and23
request/failure modes. They invoke the actual exported wrapper bodies extracted
from runt.c, generated Sys prefixes and complete owner/formatter/collector/error
source closure. Source values are rooted independently and collection runs during
construction and resolution. Tests cover nonempty old result retirement, exact
UTF String/byte-array output, H format/string arguments, missing/wrong/nil/noncanonical
tails, bad spans/count/descriptors, nil process/resolver, invalid initial capacity,
wrong live fixed Type, wrong old result identity, cancellation, resolver errors,
constructor heap/native refusal and output exhaustion. All four combinations add
4,416 gateway transactions to the existing formatter/growth/ownership gates.

A separately compiled control removes old-result identity validation and must
abort on an actual status/ownership predicate. Existing kind, native-output,
frame-pin and expected-Type removal controls remain. Whole runt.c compiles for
NDK ARM64/x64 and emits both exported symbols. This does not establish full boot,
physical target execution, all Sys I/O gateways or wire variadic admission. The
previous ASan injected-unwind and optimized UTF findings remain open.

An additional array-element removal control must detect an old Tarray whose
element identity is Tword instead of Tbyte; ordinary heap-Type equality alone
is insufficient for the aprint return contract.

H-valued old destinations are also exercised for both result kinds while
independent explicit roots keep unrelated old storage alive during collection.

A further NDK r28c riscv64-linux-android35 compile of complete runt.c, using
shared Android64 C declarations, emits ELF64 RISC-V and both gateway symbols.
This checks C compilation only; it does not establish an Android/RISC-V port.
