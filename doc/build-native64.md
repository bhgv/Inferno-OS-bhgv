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
C declarations. Forced native64-target.h checks reject a mismatched compiler or
non-LP64 data model. Hosted Linux/Android ports now provide machine-specific
assembly, FP contexts, atomics and cache publication. Linux/x64 terminal emu
links as ELF64; Android/ARM64 headless core archives build with NDK r28c.

This is not complete VM startup: installed legacy builtin tables still require
int32/ref32. Startup explicitly refuses them on LP64 before module registration.
The opt-in generated native64 binding is not yet installed for all modules.
Signal-context handling, GUI dependencies and physical-target boot remain
separate acceptance work. See native64-host-port.md for exact tests and limits.

The default scalar model remains legacy. For the opt-in WORD32/ref64 experiment,
set DISMODELFLAGS='-DDIS_WORD32=1' and rebuild every linked runtime object with
matching generated headers. The quotes are required by mk's assignment parser.
A 64-bit C target does not itself enable new Dis64 file/reference admission or
certify the complete generated native module ABI. See dis64-runtime-build.md.

utils/tests/native64-mkfiles.py uses the actual mk parser on all five profiles,
then compiles, links and executes native WORD64/WORD32 probes. The host-port
series also executes real assembly on x64 and ARM64/RV64 under QEMU, and compiles
the actual Android startup/compatibility sources for ARM32, ARM64 and x64 with
an explicitly supplied NDK. These checks do not certify a complete OS boot.

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

## Experimental ARM64 scalar-only emulator startup (INFERNO-0044)

A complete Linux ARM64 console emulator now builds with classic mk and runs
checked scalar Dis64 modules through the real OS, filesystem, pools, scheduler
and ARM64 JIT. This is an explicit builtin-free experiment, not general emuinit
or shell startup. The normal legacy builtin ABI guard remains unchanged.

Prepare a fresh output tree to avoid mixing scalar models:

```
python3 utils/configure.py --host linux --target linux --arch arm64 --console \
  --cross aarch64-linux-gnu- --native64-scalar-boot
mk mkdirs
mk emuinstall
```

Cross builds require host-executable mk, iyacc, decgen and Limbo in the tool
locations used by the existing build. The selected target compiler/archive tools
must supply Linux ARM64 headers and libraries. A native ARM64 build uses an empty
CROSS. The equivalent manual setting is:

```
DISMODELFLAGS='-DDIS_WORD32=1 -DDIS64_PILOT_LOADER=1 -DDIS_NATIVE64_SCALAR_BOOT=1'
```

Rebuild every runtime/library object coherently. The feature rejects missing or
conflicting flags at compilation. It skips all legacy builtin initialization,
rejects `$Sys` and other builtin imports, refuses native plugins and legacy Dis
images, and retains the existing 43-opcode np0 Dis64 admission. No wire IDs,
Frame64/map8 contract, execution_supported flag or SDK default changes.

`-d /dis/pilot.dis64` selects the initial module (`-i` and positional arguments
are not module selectors). `-c 0` selects interpretation; `-c 1` enables JIT.
`-v` reports scalar module termination and whether a JIT was installed.
Linux cleanexit kills its process group; isolate test runs with a new session.
The automated test does this and requires a termination marker, rather than
interpreting SIGKILL alone as success:

```
python3 utils/tests/native64-scalar-boot.py --emu /build/emu/Linux/o.emu-g \
  --root /build --runner qemu-aarch64
```

Verified with a static ELF64 AArch64 executable, Clang/LLD from NDK r28c targeting
Linux GNU, Debian ARM64 glibc 2.36 and QEMU user emulation. Both pilot42 and
pilot-init42 complete with JIT off/on; builtin and legacy image refusals pass.
A complete ordinary Inferno userspace still requires native64 builtin/module ABI
integration. This does not resolve the previously recorded UTF/error-unwind ASan
findings, nor prove execution on physical ARM64 or an Android APK.

Separate target coverage uses actual VM source closures with bounded OS/GC
fixtures, not the complete emulator:

```
python3 utils/tests/arm64-runtime.py --system Linux \
  --cc aarch64-linux-gnu-gcc --runner 'qemu-aarch64 -L /arm64-sysroot'
```

Linux/glibc and Android/Bionic ARM64 both pass 744 complete image inputs and
1,667,800 scalar runtime predicates, plus scheduler, file loader, vmachine and
exception propagation/selection controls. The Android run uses a static target
C executable, not Java or an APK.

Pool geometry now reserves enough bytes for Bhdr's five free-list/tree pointers
and Btail: a 32-byte quantum on ILP32, 64 bytes on LP64. A 32-byte LP64 block could
corrupt its successor when freed; this was reproduced during real emulator boot.
Both hosted and OS pool initializers use the shared mask. The owning hosted pool
gate checks 10,240 allocations, 5,120 reallocations, payload preservation and
bidirectional coalescing on AMD64/UBSan and ARM64/QEMU:

```
python3 utils/tests/pool-native64.py
NATIVE64_ARCH=arm64 CC=aarch64-linux-gnu-gcc \
  NATIVE64_CFLAGS='-O2 -DNDEBUG -static -fno-strict-aliasing' \
  NATIVE64_RUNNER=qemu-aarch64 python3 utils/tests/pool-native64.py
```

Two additional actual build blockers were fixed: plain mk selected an early
assembly-object rule instead of the full Linux/Android target, and a dead pthread
stack probe required an undeclared GNU extension. Private fdlibm core names now
have an Inferno prefix to avoid static glibc libm strong-alias collisions (notably
__ieee754_pow). Rebuild libmath and its consumers together; the 68-export actual
Math registration/invocation gate passes against the rebuilt numeric libraries.
