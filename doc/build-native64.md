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

# Android shared runtime tree

The Android runtime root is `/sdcard/Inferno`. Native startup validates that
this existing directory is accessible; it never creates or selects an internal
app directory. INFERNO_ROOT, INFERNO_FILES_DIR and NativeActivity data paths do
not redirect this resolver. Launcher compatibility sets EXTERNAL_STORAGE to
/sdcard only for older bundled amain implementations.

The APK repositories optionally bundle compiled runtime assets and extract
missing files directly into `/sdcard/Inferno`; the option is off by default.
Otherwise populate `/sdcard/Inferno` yourself, including dis/emuinit.dis.
Existing files are preserved and no internal runtime copy is used. If Android denies
shared-storage access, startup fails explicitly. No automatic private fallback
or claim of access on every Android version is made. Android permissions and
storage policy must be handled in the Android launcher repositories.

The previously published private-storage installer and its handoff patch were
withdrawn because they contradicted the required shared-tree location. ABI
selection and GLES fixes remain independent of this correction.

Companion source and build notes:
https://github.com/bhgv/Inferno-OS_Android/blob/master/docs/native-storage.md
https://github.com/bhgv/Inferno-OS_Android-Clutter3d/blob/master/doc/native-gles.md

Host resolver tests check fixed-root selection, denied/missing storage and
output bounds. Startup-option tests check quoting/capacity separately. These
checks do not replace an Android APK/device acceptance test.
