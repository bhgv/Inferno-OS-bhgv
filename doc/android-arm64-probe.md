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
/sdcard/Inferno/dis/native64-apk/series0059-probe.dis64. Existing exact files are
reused; different files are refused without overwriting. The rest of an existing
Inferno tree is preserved. There is no private runtime directory. Storage access
must be granted in Android settings; the native screen explains failures and
allows a tap to retry before VM entry. Diagnostics append to
/sdcard/Inferno/native64-fb-probe.log. The probe does not read or change the normal
AWM startup command.

The source program checks 4097 calls with fresh helper locals, numeric case
lowering, arithmetic and live caller Big/Real values. A visible PASS requires
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
