# Native 64-bit toolchain profiles

mkfiles now supplies Linux-amd64, Linux-arm64, Linux-riscv64, Android-amd64 and
Android-arm64. amd64 denotes x86-64; arm64 denotes AArch64. RISC-V Linux defaults
to rv64gc/lp64d. CROSS is empty for native builds or a complete tool prefix for
cross compilation. Android uses modern NDK LLVM drivers, API21 by default.
Configure the NDK prebuilt host directory and API in its selected mkfile.

Examples for mkconfig (one assignment per line):

```
SYSHOST=Linux
SYSTARG=Linux
OBJTYPE=amd64
CROSS=
```

For an AArch64 cross build use OBJTYPE=arm64 and CROSS=aarch64-linux-gnu-.
For RISC-V64 use OBJTYPE=riscv64 and CROSS=riscv64-linux-gnu-.
For Android use SYSTARG=Android, OBJTYPE=arm64 or amd64, and ANDROID_NDK_HOME.
Create the selected platform's bin/lib output directories before library builds.

These profiles select compilers, target flags, archive tools, linkers and shared
C declaration headers. Forced native64-target.h checks reject a compiler for
the wrong architecture or a non-LP64 data model. They are useful for native core/library builds and
cross-compilation preparation. They do NOT complete the missing native platform
ports: emu/Linux and emu/Android still lack the corresponding mkfile-CPU,
asm-CPU, cache-flush, FPU-context and signal-context integration. The legacy
common emu headers used by isolated core checks must not be mistaken for those
machine-specific execution contexts. A top-level full emu build is not certified.

The default scalar model remains legacy. For the opt-in WORD32/ref64 experiment,
set DISMODELFLAGS='-DDIS_WORD32=1' and rebuild every linked runtime object with
matching generated headers. The quotes are required by mk's assignment parser.
A 64-bit C target does not itself enable new Dis64 file/reference admission or
certify the complete generated native module ABI. See dis64-runtime-build.md.

utils/tests/native64-mkfiles.py runs the actual mk parser on all five profiles,
then compiles, links and executes amd64 WORD64 and WORD32 probes. Other target
compilers/NDK are not installed in the host verification environment. Public
lib9 headers forward to shared platform declarations; architecture execution
contexts remain separate port work.

# Android root selection, entirely native

emu/Android/amain.c used EXTERNAL_STORAGE/Inferno with a fixed128-byte temporary.
It now calls storage.c, accepts native launcher paths, preserves complete path
bounds and refuses unavailable storage instead of formatting a null environment
value. Startup options quote the root as one argument; trace output uses the
same selected root and closes FILE streams with fclose.

The native APK launcher reads NativeActivity data-path fields and supplies the
result through the environment. Another native entry point may call
inferno_android_set_storage(internal, external) before amain on the same startup
thread. Android activity and asset APIs remain in the separate APK repository;
no Java source or JNI method call is required. The preferred root is the internal
app directory's Inferno child, with optional app-specific external fallback.
INFERNO_FILES_DIR supplies an equivalent native environment interface.
An explicit INFERNO_ROOT is respected; an existing accessible legacy
EXTERNAL_STORAGE/Inferno is used only as fallback.

The actual APK entry and C asset deployment are maintained in:
https://github.com/bhgv/Inferno-OS_Android/blob/master/docs/native-storage.md
It stages the runtime filesystem and populates the private root before amain,
including compatibility with its bundled older Inferno startup. App-specific
storage does not grant other apps access and is removed on uninstall; this does
not bypass scoped storage for an arbitrary public /sdcard directory.

emu/Android/tests/storage.py checks32 actual filesystem/publication predicates.
The existing startup-option gate retains1542 capacity/quote cases. Those are
host checks, not a device/NDK/APK acceptance across every Android release.

References:
https://developer.android.com/ndk/reference/struct/a-native-activity
https://developer.android.com/training/data-storage/app-specific

# APK publication

The companion native APK change is published in Inferno-OS_Android master:
https://github.com/bhgv/Inferno-OS_Android/commit/ed9ac7d8406bdbf8be26aa553aaba6f8746aa860

Its exact tree is85d09b557b2378b5b14d39c318ee0627c27506ce. It contains native
startup, asset staging/deployment, ABI selection, host tests and documentation.
It introduces no Java source or JNI calls. Repository access was granted and
publication completed; the earlier integration403 block is resolved.

[Inferno-OS_Android-native-storage.patch](patches/Inferno-OS_Android-native-storage.patch)
is retained as a reproducible handoff from the old master
3b09124172446fca6a4c843b78601b7dc3cfc90d. Do not reapply it to the updated master.
Host native-storage and CMake configuration checks passed; a complete APK/NDK
build and device execution remain unverified in this environment.
