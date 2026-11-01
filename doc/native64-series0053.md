# Series 0053: complete scalar console execution on ARM64, x64 and RV64

This series advances the existing experimental Linux console, not full Limbo
shell or Android GUI startup. It preserves the shared Dis64 v1 np0 scalar file
profile, its 43 admitted operations and all existing wire identifiers. Complex
operations keep their C handlers. No file CALL/MCALL or native import admission
is added.

## Corrected terminal error behavior

The scalar-only console inherited `keepbroken=1`. A program that raised a VM
error therefore entered Pbroken and waited for a debugger indefinitely, although
this startup profile admits neither the ordinary shell nor its debugger. The
previous complete x64 executable reproduced a successful arithmetic image and
then hung after reporting the deliberately incorrect image's `zero divide`.
The test stopped that isolated process group after three seconds.

Only DIS_NATIVE64_SCALAR_BOOT now selects keepbroken=0. Ordinary Inferno retains
its original debugging default. Errors still print the program and diagnostic,
then follow the actual destroystack/delprog cleanup and final-host-exit path.
Verbose scalar startup also reports an empty scheduler after program retirement.
The boot gate requires that late marker; a pre-cleanup message alone is no
longer accepted as successful termination. Host cleanexit deliberately kills its
own isolated process group, so the harness continues to accept that status only
with the required program/retirement evidence.

## Full operating-system processes

native64-boot-programs.py independently serializes established scalar sections
and embeds result checks into the bytecode. Python integer arithmetic supplies
expected modular results; a mismatch branches to a deliberate divide-by-zero
operation. One altered expected result is an active negative control in both
interpreter and JIT modes.

The corpus executes every one of the 43 currently admitted scalar operations:
20 WORD/LONG arithmetic and bitwise operations, all 18 scalar branches, three
moves, JMP and EXIT. This is selected operand coverage, not every state of each
operation. Cases include signed extrema, MIN/-1, large shifts, both outcomes of
all six WORD/LONG/Real predicates, finite values and infinities, sign-extended
LONG immediates, memory moves, signed zero and exact quiet-NaN bit transport.
Two loops execute 4097 iterations, exceeding the default 2048-instruction quantum.
Division/remainder by zero and invalid shifts exercise real terminal errors.

NaN ordering is not a portable success case in this full-process profile:
startup enables invalid-operation traps where supported, and C ordered NaN
comparisons can trap. The full-process test does not mask FP exceptions to force
such a pass. Exact NaN MOVF transport is tested without floating comparison;
previous dedicated FP/fallback gates retain their separate scope.

On each architecture, 19 corpus images run with JIT0/1: **38 full OS processes,
1096 checks**, including the wrong-result control and error completions. The
original four pilot processes and three closed-admission checks also pass.
The emulator uses its real Linux OS, pools, scheduler and executable JIT pages;
these runs do not replace the OS with a fixture.

| Execution | Whole console build/run | Additional corpus | Native hardware evidence |
| --- | --- | --- | --- |
| ARM64 Linux | classic mk, ELF64 AArch64, QEMU | 38 processes / 1096 checks | QEMU, not physical ARM64 |
| x86-64 Linux | classic mk, ELF64 x86-64, native | 38 processes / 1096 checks | Native host |
| RV64 Linux | classic mk, ELF64 RISC-V, GNU RV64GC/lp64d, QEMU | 38 processes / 1096 checks | QEMU, not physical RV64 |

Math and the selected eight Sys I/O exports are registered as trusted C startup
modules. This remains registration evidence in whole-process runs; files still
cannot import those modules. Existing protected C-call, field witness, allocation,
GC and link-coherence gates provide their separately described evidence.

## RV64 lifecycle coverage and reproducibility

The former arm64-runtime driver is now native64-runtime.py with an explicit
--arch arm64/riscv64 option. arm64-runtime.py remains a compatible entry point.
The shared driver executes 744 scalar images and 1667800 actual C/RX predicates
on each cross target, plus actual scheduler, file loader and vmachine closures.
Those closures retain bounded OS/GC services; they are distinct from the full
console processes above. Their no-propagation/no-handler-selection controls
still reach active failures. An RV64 compiler guard rejects a wrong target.

native64-live-vm.py also selects the actual RV64 compiler backend. Direct and
file-loader entry/return/unwind cases and both enter/unwind removal controls
pass under RV64/QEMU. The clean port driver now selects these lifecycle and
runtime gates for RV64 as well as ARM64. Runtime C flags honor the dedicated
NATIVE64_RUNTIME_CFLAGS override, the older ARM64_RUNTIME_CFLAGS override, then
the port driver's NATIVE64_CFLAGS.

Clean builds of all three consoles passed their selected boot, partial-link,
header invalidation, protected Math, GC lease/live/map and field-witness gates.
Final program-retirement changes were relinked and the extended full-process
corpus rerun on all three. A further clean ARM64 build exercised the consolidated
port driver. Native x64 owning C gates used O3/NDEBUG strict UBSan; cross ARM64
and RV64 owning gates used O3/NDEBUG without sanitizer instrumentation. The GNU
RV64 sysroot used here lacks libubsan, and no RV64 UBSan acceptance is claimed.
Whole-emulator flags remain those of the classic-mk profiles.

Example, using native source-built mk/limbo/iyacc/decgen:

```
python3 utils/tests/native64-port.py --arch arm64 \
  --build-root /build/arm64 --host-tools /build/host-tools \
  --cross /tools/aarch64-linux-gnu- \
  --test-cc /tools/aarch64-linux-gnu-gcc-dynamic \
  --runner 'qemu-aarch64 -L /sysroot/aarch64-linux-gnu'
```

For RV64 use --arch riscv64, the GNU riscv64-linux-gnu- prefix and a matching
qemu-riscv64/sysroot. GNU --wrap=free fixture gates need the dynamic libc compiler;
the complete emulator may use a separately selected static-link compiler prefix.
Do not reuse ABI objects from another profile. The driver requires an empty
external build directory and keeps complete logs under port-logs.

## JIT statistics and remaining work

Emitter readiness is unchanged: ARM64 and x64 each have 173/173 complete JIT+C
entries, with 166/173 guarded native fast paths (95.95%). The seven C-only cases
stay in C. RV64I has 87 native fast paths (50.29%), Zmmul 90 (52.02%), and M 103
(59.54%); complete JIT+C entries remain 173/173 for all variants. These enum-wide
figures do not widen the 43-operation file loader or measure whole-port readiness.
Per-command evidence remains in libinterp/tests/README.jit-readiness.md and its
CSV; jit-readiness.py --check passes.

Full Limbo compiler-to-Dis64 application startup, generic managed native calls,
file CALL/MCALL, shell/builtin import admission, detached/suspended caller lifetime
and physical ARM64 validation remain open. Earlier optimized UTF/error-unwind
ASan reports remain open. No Tk, security, crypto, MIPS, SPARC or PowerPC source
is changed.
