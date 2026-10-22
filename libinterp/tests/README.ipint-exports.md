# IPints export, ownership and base64 regressions

Run from the repository root:

```sh
python3 libinterp/tests/ipint-exports.py
python3 lib9/tests/base64-bounds.py
python3 libmp/tests/text.py
```

The IPints fixture compiles actual export wrappers, the owned constructor,
division/modulo wrappers, retstr/mem2array, and emu's kprotect/krethrow bodies.
It uses real Proc, Heap, String, Array, IPint and generated builtin-frame layouts.
The native side links production portable libmp arithmetic/formatting and lib9
codecs, with the tracked allocator from the libmp text fixture. The VM heap and
ASCII string constructor are bounded stubs with deterministic VM-error injection.

Its 211 cases include zero, signed magnitudes, both compatibility-prefix branches,
and sizes around 768/1024 bytes up to 4096 bytes. All five export wrappers retain
old results when validation, native-buffer allocation or VM output allocation
fails. Native temporaries and VM references must balance after each case. Tests
cover nil/wrong-type references, impossible formatting lengths, cleanup after
both quotient/remainder heap failures, atomic pair publication, owned constructor
failure, zero-divisor VM errors and successful division/modulo results.

IPints no longer exports through fixed 1024-byte stack buffers. Ordinary base64
and textual exports retain libmp's signed representation; byte exports and the
legacy base64-with-leading-zero form retain their magnitude-only behavior. The
leading zero is inserted only when the highest magnitude bit is set. Native
buffers are released on success and VM unwind, while the runtime rethrows the
original error. newIPint now consumes its supplied magnitude on VM allocation
failure as well as success. Existing tuple-producing callers beyond IPint.div
are not exercised by this fixture.

The base64 codec fixture compares 31,538 oracle/capacity/guard cases against
Python's base64 implementation. It preserves tolerated whitespace and partial
output behavior while rejecting nonpositive output capacity before any write.
Negative counts no longer decrement INT_MIN. libmp's base64 formatting helper
now returns failure for undersized caller buffers instead of reporting a
truncated string as a successful conversion; text.py checks that boundary.

All tests default to UBSan with immediate failure. The new fixtures also fail
against the original production functions. A full stack-instrumented ASan run
cannot be counted as passing in this sandbox: ASan cannot obtain stack bounds
for VM siglongjmp unwinds and leaves stale redzones after caught exceptions.
Heap/global ASan plus UBSan can be run with GCC as follows:

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer --param=asan-stack=0' python3 libinterp/tests/ipint-exports.py
```

That configuration passes; stack instrumentation is explicitly disabled.
LeakSanitizer process inspection is also disabled because `/proc` is unavailable;
independent native-allocation and VM-reference accounting remains mandatory.
Fatal native mpnew/mpbits allocation failures, collection cycles, complete Inferno
boot and native signal-to-VM-error translation are outside this fixture.
Active Dis layouts and builtin signatures are unchanged; export sizes are derived
from magnitude bits rather than a fixed native buffer or target pointer width.
