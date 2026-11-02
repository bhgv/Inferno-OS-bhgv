# Android ARM64 framebuffer Dis64 probe

The `experiment` Android ARM64 target can build an explicitly restricted scalar
runtime archive with classic mk. It uses Bionic, the real ARM64 JIT, pthread TLS
and native64 Frame geometry; it does not register legacy int32/ref32 builtins.
The separate framebuffer APK launcher is in Inferno-OS_Android under
`app/src/main/cpp/arm64-probe`. It requires no application Java or DEX.

Prepare a clean checkout and host mk/limbo/iyacc/decgen, then configure:

```
python3 utils/configure.py --host linux --target android --arch arm64 \
  --renderer framebuffer --native64-scalar-boot --ndk /path/to/android-ndk \
  --root /absolute/clean/checkout --output mkconfig
mk CONF=emu-scalar /absolute/clean/checkout/Android/arm64/lib/emu.a
```

The NDK must provide the modern LLVM command names used by
mkfiles/mkfile-Android-arm64. Use clean objects and archives: changing CONF in an
existing emu.a can leave old archive members, and changing model flags does not
make incompatible objects reusable. The APK build script starts from a clean
checkout, records exact source/NDK revisions and selects emu-scalar explicitly.

Only Android ARM64 framebuffer is admitted for this probe. GLES, trusted native
Math/Sys I/O registration and caller-guard combinations remain rejected here.
Linux's existing console configurations are unchanged. The dedicated C entry
uses the portable parser and layout gate; it cannot reboot into a second VM.
Scalar-profile Android formatting additionally writes redirected stdout/stderr
while retaining logcat messages, so the native UI can verify this session's
actual module completion and scheduler retirement.

The APK installs only its bounded probe image into
/sdcard/Inferno/dis/native64-apk/series0067-probe.dis64. Existing exact files are
reused; different files are refused without overwriting. The rest of an existing
Inferno tree is preserved. There is no private runtime directory. Storage access
must be granted in Android settings; the native screen explains failures and
allows a tap to retry before VM entry. Diagnostics append to
/sdcard/Inferno/native64-fb-probe.log. The probe does not read or change the normal
AWM startup command.

The current source program checks4097 result-producing calls with fresh helper
locals, numeric cases, nested Word result forwarding and Big/Real result values. A visible PASS requires
the correct named module, requested JIT selection, empty exception text and an
empty retired scheduler in this session's log. Interpreter and JIT APK variants
use the same exact image and have separate application IDs.

Local NDK r16b ARM64/Bionic compilation/linking succeeds. The static Bionic CLI
adapter passes 38 whole interpreter/JIT processes and 1096 in-program checks
under QEMU, including wrong-result controls and rejection of legacy images and
native Sys/Math file imports. Its Android logging and dynamic-symbol adapters
are test stubs; actual VM execution, handlers, JIT and Bionic pthread/memory
services are not replaced. This is not a NativeActivity, device or GPU test.
Modern NDK APK CI and download results are recorded separately after completion.

This is an Android ARM64 execution probe, not a complete AWM desktop. General
native64 imports, managed argument/result ABI and full GUI startup remain work.
The shared 43-op np0 Dis64 file profile and FlashVM format IDs do not change.

Modern Bionic static linking initially exposed duplicate strtoll/strtoull
definitions. The Android ARM64 declaration wrapper now gives Inferno's existing
converters private names, renaming their definitions and callers together.
Bionic keeps its standard symbols. No multiple-definition linker waiver is
used. The ARM64 mk profile tracks its declaration wrapper as an owning input.

## Published ARM64 APKs

[Android build 41354049175](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41354049175)
passes with NDK r27c, core56f112e and launcher ebfdea0. Download the
[APK and evidence ZIP](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41354049175/artifacts/13102686436).
It contains two signed native-only AArch64 APKs:

| APK | Mode |
| --- | --- |
| Inferno-FB-arm64-v8a-probe-jit0.apk | Interpreter |
| Inferno-FB-arm64-v8a-probe-jit1.apk | ARM64 JIT |

Both pass actual ELF64/AArch64 inspection, 16KiB LOAD alignment and APK
signature verification. The downloaded copies also match their checksums,
contain no DEX and carry the exact same checked image, SHA256
`e15cabfa3bed16d746bedf2b2fd0b6f4c4b5a6418b27f545a952001eaf9bcff2`.
The expected native screen is PASS: ANDROID ARM64 DIS64. Grant shared-storage
access if requested by the screen; preserve native64-fb-probe.log for feedback.
GitHub artifacts require login and follow repository retention.

Modern static Bionic execution uses an API29 ELF-TLS test harness with the
required 64-byte alignment and an active initial-thread/VM-worker isolation
witness. It passes 38 whole C/JIT processes and 1096 in-program checks, plus
legacy/import refusals. The exact packaged image separately retires with both
execution modes before packaging. These CLI tests adapt only Android log-daemon
and symbol-lookup services and CLI completion. The APK has its real Android
loader and framebuffer path; its minimum SDK remains21. Physical phone startup,
permissions, presentation and SELinux JIT execution remain device checks.

Host production-function tests cover stream reads, exact reuse/no overwrite,
access refusal, cleanup/retry, prior-session result rejection and a deterministic
pending-worker double-tap race. Retry state is cleared before a new worker can
enter the VM, so a second tap cannot join that running worker and hang the UI.
Posting to the native window pauses on focus loss. A real Activity teardown
terminates this dedicated process rather than leaving VM workers with a stale
native Activity. Normal ARM32 FB/GLES entry points are unchanged.

Native Linux ARM64/x64 and RV64/QEMU regression CI also passes at core56f112e:
[run41352969207](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41352969207).
A later diagnostic-only source cleanup clears a borrowed signal context when
the backtrace log cannot open; its AArch64 Android object compiles. The APKs above
retain their recorded exact core pin and disable the Inferno signal traps.


## Device-reported retirement failure and corrected APKs

The first phone logs reproduce `alloc:D2B ... in free blk` after the module
reports `exit=""` and an empty scheduler, in both JIT0 and JIT1. The APKs from
build 41354049175 therefore do not pass complete worker retirement. The old CLI
adapter exited at the splash callback, before production `pexit`, and missed
this failure.

Core commit `0a7cb57f00482117007a49f7f41c718d0075cae3` detaches the worker's
borrowed `Osenv` before `delprog` frees it. Otherwise Android `pexit` reads the
freed environment and frees its user string again. The corrected scheduler
fixture checks the pointer before process free and host exit, instead of
repairing it in the test. Removing the production fix fails the control.

The revised Bionic CLI adapter returns from the completion callback and waits
for actual `pexit` to reach `pthread_exit`. An ARM64/Bionic r16b QEMU replay of
the exact bundled module reproduces the allocator panic with the old core in
both modes; the corrected core exits zero in both modes. Symbolization locates
the old caller in `pexit` at `free(e->user)`. This is runtime cleanup evidence,
not physical Android UI validation.

[Corrected build 41357793945](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41357793945)
passes NDK r27c compilation, host launcher gates, 38 Bionic/QEMU processes with
1096 in-program checks, import refusals and both exact packaged-image runs,
including real worker cleanup. Launcher pin: `9a893441692d3c21ac82df81e34e6660e7915078`.
[Download the corrected APK pair and evidence](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41357793945/artifacts/13103996704).

| APK | SHA256 |
| --- | --- |
| Inferno-FB-arm64-v8a-probe-jit0.apk | ff7610c2ac9512c875b11eea3799c4a3b644993052990ade1a42b47334a5fd6b |
| Inferno-FB-arm64-v8a-probe-jit1.apk | 55f7a00a73ae222cc80fa93e4a4411e53e2f8277c5c7664dbb3ab4d7ba9e99f4 |

Downloaded files match their checksums, contain ELF64/AArch64 native libraries
with 16KiB LOAD alignment, no DEX and the unchanged shared probe image. These
remain scalar framebuffer probes, without AWM. Physical-phone confirmation of
the corrected retirement path is pending.


## Series0067: current compiler and ARM64/Bionic results

ARM64 APK packaging already works. A complete AWM client is still a separate
acceptance target: the scalar probe does not admit general runtime file calls,
managed imports or the native64 GUI builtin closure. The remaining work is not
measured by the166/173 ARM64 fast-opcode count.

| Layer | Confirmed state | Remaining boundary |
| --- | --- | --- |
| Android ARM64 packaging | Two signed native-only FB APKs; ELF64 and16KiB LOAD alignment | Physical installation/window/storage acceptance of this new pair |
| VM and JIT on Bionic | Whole QEMU processes, original VM/JIT/libc and worker cleanup | Physical phone execution of the updated corpus |
| Scalar Limbo-to-Dis64 | Bounded arguments, expressions, nested calls and int/big/real results | General runtime file CALL/FRAME and managed values |
| Native module registration | Restricted scalar startup; explicit import refusal | Shared ABI64 builtin/call/GC ownership across required modules |
| AWM desktop | Existing runtime tree preserved | Full managed/GUI startup and framebuffer presentation together |

Core4a910d74001e6868e35c7bc69a0d86afcdec4fdd fixes the production Draw call to
poolmutable by including pool.h and adding its classic-mk dependency. NDK r27c
had stopped at the missing declaration; compiler diagnostics are not suppressed.
The source retains0066 scalar results and all earlier native64 fixes. Launcher
7a60a15d46bf4690bbd8693b76edf11e9efe9a4f also rejects panic diagnostics after
completion markers; a host production-function removal control exposes the
old false-success result. Package IDs and the shared image path are versioned
for0067, with exact reuse/no overwrite and no private runtime tree.

[Android build41398236383](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41398236383)
passes with NDK r27c and both exact packaged-image C/JIT runs including actual
worker retirement. ARM64/Bionic QEMU additionally executes167 helper/result
sources /334 whole C/JIT processes plus8 active VM controls and16 refusals;
67 numeric-case sources /134 processes with67 stale-PC controls; and the prior
boot corpus. Native Linux ARM64/x64 and RV64/QEMU regression
[CI41398170110](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41398170110)
also passes at that core pin.

[Download the two current APKs and complete evidence](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41398236383/artifacts/13119948900).

| APK | Mode | SHA256 |
| --- | --- | --- |
| Inferno-FB-arm64-v8a-series0067-jit0.apk | Interpreter | 2dbde4056d2399d1a65fedf4ee378add6bfda64f1410e37525e1776a228861d2 |
| Inferno-FB-arm64-v8a-series0067-jit1.apk | ARM64 JIT | 0a5b7562570899e145aba2e12da3a8f2bd4c6bc9e61fddbf241b57f46ff5b2db |

Downloaded copies are4608846 bytes each, match checksums, contain the expected
ELF64/AArch64 library with16KiB LOAD alignment, no DEX and identical probe image
SHA2569138ae33fb041adb8b0b689ad1e5c1420fdb5d86a33f0c4dad6c6e96b05c18e0.
CI verifies v1/v2/v3 signatures and zip alignment. The expected native screen is
PASS after4097 returned calls and all checks, with logs under /sdcard/Inferno.
These new APKs still need physical-phone confirmation; QEMU is not NativeActivity,
GPU, permission or16KiB-page-device acceptance. No shared format allocation or
new managed file-import admission is introduced.
