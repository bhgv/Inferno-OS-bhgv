# Classic hosted builds and terminal profiles

The build remains `mk`. Python is optional configuration preparation and never
replaces compilation, archive creation or the platform mkfiles.

Examples, from the repository root:

```
python3 utils/configure.py --host linux --arch 386 --console
mk mkdirs
mk emuinstall
mk install
```

Use `--host windows --arch 386` for the existing Nt/MSVC profile, or
`--host macos --arch 386` for the existing MacOSX profile. Add `--console` to
select the terminal emulator. Omit it to restore the platform's ordinary entry.
The script preserves unrelated mkconfig settings and atomically writes its
selected values. `--output /path/to/config` previews another file without
changing the active mkconfig. Classic mk paths must not contain whitespace or
mk metacharacters. Windows ROOT uses drive:/path notation.

Console configuration lives in mkfiles/profile-console. The emu/Linux,
emu/MacOSX and emu/Nt mkfile-console entries invoke the original platform rules
with CONF=emu-g and CONFLIST=emu-g. This omits GUI devices and modules; the root
build profile omits GUI library build directories. macOS console linking omits
Carbon/QuickTime graphics frameworks. Terminal startup can run, for example,
`emu-g -r/path/to/Inferno /dis/sh.dis` (emu-g.exe on Windows).

The existing Linux/MacOSX emu-g device files are reused. Windows gains an
emu-g configuration with its native network devices and console subsystem
entry. Normal platform mkfiles remain the source of compilation rules.

## Supported profiles versus completed ports

| Platform | Existing historical emulator CPU profiles | New 64-bit toolchain profiles |
| --- | --- | --- |
| Linux | 386, arm and other existing ports | amd64, arm64, riscv64 |
| Windows / Nt | 386, using the MSVC/rc toolchain | No native Nt/amd64 emulator port yet |
| macOS / MacOSX | 386 and power with compatible historical SDKs | No native MacOSX/amd64 or arm64 emulator port yet |
| Android | arm with its standalone GCC toolchain | amd64 and arm64 |

A profile is not a substitute for missing CPU assembly, FPU, signal/context or
OS API implementations. configure.py refuses nonexistent profiles and identifies
64-bit toolchain selections as incomplete emulator ports. Windows and macOS were
not compiled on this Linux host. Existing legacy macOS profiles require compatible
old SDK/compiler support; current Apple Silicon support is not claimed.
Linux/386 requires a working 32-bit compiler and system development libraries.

## Android renderer ABI

```
python3 utils/configure.py --host linux --target android --arch arm --renderer framebuffer
# or: --renderer gles
```

Both use WINDOW_BACKEND=andr. Framebuffer leaves EXTWINFLAGS empty; GLES sets
it to -DEXT_WIN. Rebuild every runtime object when changing this flag, preferably
in separate checkouts. External-window Memimage operations cannot be mixed with
software framebuffer compositing. The 64-bit Android profiles use the same
explicit setting. Android/arm still expects a prepared standalone toolchain at
ndk_toolchain; --ndk supplies ANDROID_NDK_HOME for the LLVM 64-bit profiles.

Actual ARM test builds used NDK r16b, API 17, two separate renderer checkouts,
classic mk emuinstall, and mk install in appl. FreeType hosted headers now use
compiler ABI types instead of Android-specific typedef guesses. Android, Linux and macOS supply
a real inter-thread coherence fence. The Limbo Android link includes libm.
Host mk temporary command-line assignments honor TMPDIR, including the mk variable
table populated before the original process environment is split during import.

## Validation

`python3 utils/tests/configure.py` checks three console platform selections,
actual generated device lists, distinct Android renderer flags and refusal of
missing ports while preserving the active mkconfig. The native bootstrap test
builds and executes the real Linux/amd64 host tools. Actual Android ARM libraries,
the runtime Dis tree and both native APK libraries were compiled and linked.
This does not certify Windows/macOS compilation or an Android device run.

Gaming-console targets are deliberately excluded from this series.
