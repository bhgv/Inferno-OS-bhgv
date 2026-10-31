# Complete explicit native64 Math registration

Math is the first complete builtin module wired to builtinmod64. This is an
explicit experimental C build option, not a change to installed defaults or
Dis64 native-call admission. Existing header-only consumer mode still refuses
registration unless the extra option is selected.

Generate a fresh header with the owning Limbo compiler, compile math.c with
DIS_WORD32=1, DIS_NATIVE64_MATH_HEADER naming that header and
DIS_NATIVE64_MATH_REGISTER=1. All runtime objects still need the same scalar
model. Values0/2 and registration without a generated header are compile errors.
Neither an installed legacy table nor a mismatched model is used as fallback.

## Producer and initializer

LIMBO_NATIVE64_IMPLEMENTATIONS requests a complete immutable pointer list to
existing fixed-frame implementation records, plus an explicit logical count.
It implies specification tables and individual records/maps. This differs from
callback-free LIMBO_NATIVE64_TABLES: the implementation list deliberately links
the complete module's C implementations. Variadic prefixes are excluded. An
all-variadic module has zero logical implementations and one nil storage cell;
ADT-qualified method names retain their distinct records. These cases are
checked using fresh generated C. Output remains deterministic.

mathmodinit checks the generated model/layout and registers all68 implementations
against all68 specifications through the actual builtinmod64 constructor. It
installs numeric formatters only after successful module publication. Failed
construction is reported through the VM error path; no partial module or
formatter installation escapes. Legacy/default initialization remains unchanged.

This is trusted native C metadata, not a structural signature accepted from an
untrusted Dis file. No format IDs, serialized payloads or hash semantics change.
No iyacc dependency is imposed on FlashVM's independent frontend.

## Actual execution and reproduction

First build native Linux/x64 numeric libraries with classic mk, using the host
port configuration described in native64-host-port.md. Supply the source-built
library directory and a matched fresh Limbo explicitly:

```
python3 utils/tests/native64-math-register.py \
  --limbo /path/to/source-built/limbo \
  --libdir /path/to/Linux/amd64/lib
```

The test links complete owning math.c, real classic-mk libmath/lib9 and actual
constructor/cleanup bodies reused from native64-builtins.py. It supplies bounded
OS allocation/error transports and canonical primitive identities. Formatter
installation is observed through a link wrapper; the formatter body itself is
not executed. The owning numeric bodies and dependencies are not replaced.

Both WORD32 and WORD32+file register and invoke every one of the68 published
exports through Module.ext, not by directly calling a generated entry record.
Known numeric results check unary/binary operations, mixed int/big/real frames,
int32 and naturally aligned tuple results, all vector/conversion functions,
sorting indices, four FP APIs and a real2x2 column-major GEMM. All68 entries must
be observed exactly once in the invocation matrix. This is representative
execution of each function, not exhaustive numerical/domain validation.

For each model141 allocation failures exercise the real initializer/constructor
and error owners. Complete maps/callbacks/names are checked after successful
construction; every refusal restores the original module chain, error depth and
exact live allocation count, with no formatter installation. Every successful
module retires through ordinary unload. Raw borrowed arrays are temporary C
objects at the checked Math boundary; this fixture does not execute a collector.

The gate passes O1/strict UBSan and O3/NDEBUG/strict UBSan. The owning Math and
constructor gate also passes address+undefined sanitizers with explicit
NATIVE64_NO_LSAN for unavailable host process-inspection leak scans. Live-owner
checks stay active. The numeric dependency archives are explicitly prebuilt,
so this is not whole-library sanitizer coverage. Existing host ASan executable-
name/longjmp warnings and earlier recursive-unwind limitations remain.

Set NATIVE64_MATH_LIBDIR to include the gate in native64-layout.py; otherwise the
suite explicitly skips this additional classic-library integration. Ordinary
selected Math/builtin/GC tests continue independently without those libraries.
Fresh complete suite with this integration passes O3/NDEBUG/strict UBSan,
including qualified methods, empty implementation lists and legacy regressions.
The new initializer also compiles with NDK r28c for Android ARM64 and x64;
that does not establish Android target execution or APK readiness.

## Remaining boundary

Sys and the remaining builtin closure still require coherent native64 headers,
registration and call/result ownership. Sys variadic tails and shared structural
reference/call signatures remain pending agreement with FlashVM. The default
full-startup legacy-ABI refusal remains enabled. No Dis MCALL/JIT/full-VM boot,
physical ARM64/RV64 execution, managed-reference file admission or NC64 default
change is claimed here. Complex numeric handlers stay in C; opcode fast-path
counts remain unchanged.
