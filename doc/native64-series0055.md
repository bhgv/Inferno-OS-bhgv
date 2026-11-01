# Native64 series0055: fault identity, optimized predicates and reproducible consoles

Baseline: experiment4fd4220. Read FLASHVM-0050 and built the external reader from
FlashVM8472038d2090e59c4dfe36f452bfd3b5ac3b3d4a. This series preserves the shared
43-op np0 wire profile and all native opcode availability counts.

## Changes

* The scalar console reports an original decoded Dis-PC for terminal errors,
  after checking its current program extent. PCs remain independent of native
  leaf code addresses. Success produces no fault-PC marker.
* The successful scalar compiler optionally exports exact PC/opcode/source rows.
  Private offline tooling binds them to the complete image hash and publishes a
  checked source map. It preserves UTF-8/escaped source names, explicit unknown
  endpoints, and the compiler's unmodified private cursor values. Source/image/map
  aliases refuse before compiler invocation. No VM loads this JSON.
* The Sys UTF fixture uses active `if` predicates instead of assertions disabled
  by NDEBUG. Both WORD models execute a deliberately wrong production-result
  control and require semantic rejection. A target executor is explicit.
* The clean classic-mk port driver includes source-fault and UTF gates. GitHub
  Actions builds native x64/ARM64 consoles and an RV64 console through QEMU,
  preserving executables, a portable scalar example/map and complete gate logs.
  Native ARM64 runner results must be inspected separately; local QEMU results
  alone do not establish them.

See [the private tooling and proposed shared boundary](dis64-source-locations.md).
No shared debug section, entry record, extension or registry ID is assigned.

## Executed local evidence

Clean classic-mk x64, ARM64 and RV64 console driver runs all pass. x64 is native;
ARM64 and RV64 use explicit GNU target compilers and Linux QEMU/sysroots.
Each architecture retains the38-process/1096-check hand-encoded corpus and30
source-compiled processes; the new fault-location gate adds ten complete OS
processes, including word/big divide errors, invalid shifts and normal retirement
with JIT0/1. All eight fault runs resolve to the actual source statement, and
successful exits have no fault marker. Independently checked hash/PC/opcode/count/
location mutations refuse, as do failed compilation and input aliases.

Each target's two UTF scalar models pass1,115,061 cases apiece with active
predicates, plus both wrong-count semantic controls. Native x64 uses O3/NDEBUG
strict UBSan; cross targets use O3/NDEBUG without sanitizer instrumentation.
ASan+UBSan alignment-only checks and controls pass. The complete optimized
UTF error-unwind ASan report still reproduces and remains open.

Actual ARM64 ELF64 mk/iyacc/decgen/limbo tools also build and run under QEMU.
Their scalar compiler retains byte-identical products relative to native x64
for the existing15-source corpus, which additionally runs30 ARM64 emulator
processes. The location gate also executes using that ARM64 compiler product.

The independently built FlashVM reader accepts and exactly rewrites64 live-local
layouts produced by current Inferno Limbo;64 managed-map mutations refuse.
Those layouts additionally execute in128 ARM64 emulator processes, plus two
wrong-answer retirement controls. This consumes the incoming cross-reader gate
without changing its oracle or treating reader acceptance as general execution.

ARM64/RV64 owning runtime/scheduler/vmachine/file and active removal controls,
caller/GC/field leases, partial-link and header-rebuild gates remain passing.
Each cross scalar runtime retains744 images and1667800 actual C/RX predicates;
its bounded OS/GC services remain distinct from complete emulator processes.
`jit-readiness.py --check` passes: ARM64/x64 native166/173, RV64I87,
Zmmul90, M103; complete JIT+C173/173 for each profile.

Full shell, managed imports/calls, detached/suspended caller ownership and
physical phone validation remain open. No Tk, security, crypto, MIPS, SPARC or
PowerPC implementation is changed. Complex operations stay in C.

## Native Linux ARM64 CI acceptance

[Run41334049753](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41334049753)
at0b1ae505b4f56ba5aeb405d1a0258964f710be82 passes all three jobs. ARM64 runs
natively on an aarch64 Linux GitHub runner; x64 is native and RV64 uses QEMU.
ARM64 native mk/iyacc/decgen/limbo bootstrap, complete classic-mk console,
C/JIT boot/retirement/source-location gates, UTF predicates/controls, caller/
GC/field gates,744-image C/RX runtime and external-reader gate all pass. This
adds native ARM64 Linux execution beyond the local QEMU evidence above. It is
not physical Android/phone, GUI or unrestricted managed-application acceptance.

The workflow preserves Linux scalar executables with loop.b, loop.dis64,
its diagnostic map, README and complete logs. Artifacts are available for14 days:

* [ARM64](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41334049753/artifacts/13094467223)
* [x64](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41334049753/artifacts/13094741584)
* [RV64](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41334049753/artifacts/13094491188)

The actual downloaded ARM64 archive was extracted separately and its included
self-checking loop executed again with JIT0/1 under Linux/QEMU, requiring clean
module completion and late scheduler retirement. The archive contains a Linux
ELF64 executable, not an Android APK or a complete Inferno runtime tree.

[Full UTF sanitizer run41334449491](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41334449491)
at90ebeda5b0cb513835986a0e1faa706273151a02 also passes both native ARM64/x64
jobs. Each executes both WORD models with O3/NDEBUG ASan+UBSan, all1,115,061
cases per model and both wrong-count controls. Address/undefined checks are not
suppressed. Existing fixture LeakSanitizer process-inspection exclusion remains.
The local sandbox's stack-top-zero/error-unwind report still reproduces; native
CI success narrows that discrepancy but does not prove its precise cause or
close every previously recorded injected-unwind fixture.
