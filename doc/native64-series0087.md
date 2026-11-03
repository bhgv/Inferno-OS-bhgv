# Native64 series 0087: current ARM64 Android execution pipeline

The Android framebuffer probe pipeline still pinned series0067 core4a910d74.
Updated it to owning experiment source81385bb23436185ecd5922714899e73a2163e5cb,
including the newer result-address, published LEA/cache and binding-mode guards.

Android source commit0f90b8fb72cd29fd5a06cffedb4f75e541037fc5 is published in
`experiment-arm64-series0087` of Inferno-OS_Android.
[Draft PR10](https://github.com/bhgv/Inferno-OS_Android/pull/10) keeps this work
separate from the Android default branch. No merge has been performed.

Added actual-body static Bionic/QEMU binding and Loader mode tests, WORD32/64
with three binding controls and one loader control. Existing classic-mk NDK r27c
compilation, compiler case/helper/result executions, exact packaged-image C/JIT
runs, real worker retirement, ELF64/AArch64,16KiB alignment and signing checks
remain in the build. The new static C gates are not sanitizer runs or WORD64
shared-wire execution. Versioned APK identities and shared probe image names
avoid accidental reuse of older probe content. No Java application source added.

Local host production-function tests pass reuse/no-overwrite/short-read/refusal/
retry/directory/session/result and pending-worker double-tap cases. Active panic
control and shell syntax pass.
[Android run41555685980](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41555685980)
is in progress; no new APK success/download is claimed before it finishes.

These are scalar framebuffer probe APKs, not full AWM/managed GUI startup.
Runtime storage stays /sdcard/Inferno. Physical phone behavior remains separate.
Main Linux source81385bb2 still has its own full target matrix in progress.
Focused native ARM64/x64 ASan/UBSan for0086 has completed successfully.

No runtime opcode or shared Dis64 identifier is added in0087. ARM64/x64 remain
166/173 native paths with all173 available through native plus C; RV64I/Zmmul/M
remain87/90/103 with all173 through native plus C. Production file admission is
still43/173; general file CALL/LEA8 and managed ABI closure remain separate work.
