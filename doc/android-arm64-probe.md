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
