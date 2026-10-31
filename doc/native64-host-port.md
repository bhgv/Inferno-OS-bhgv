# Hosted LP64 port: execution boundary

This series supplies hosted machine primitives for Linux amd64, arm64 and
riscv64, and Android amd64/arm64. It does not enable the installed legacy
builtin tables on LP64 and does not assign a new Dis64 format or admission.
Complex VM handlers remain in C; no opcode fast-path count changes here.

## Production changes

The five host profiles select actual emulator contexts, assembly and cache
publication instead of borrowing a 32-bit machine context. Common execution
uses pthread TLS and sigsetjmp with signal-mask preservation. FP contexts own
x87/MXCSR, FPCR/FPSR or FCSR; C/JIT calls must still obey the C register ABI.
Multiplication returns both halves of a full unsigned-long product; atomic
exchange touches exactly one 32-bit Lock word. ARM64 uses baseline acquire/
release exclusive operations; RV64 uses the atomic and double-float ABI of
rv64gc/lp64d. Cache publication uses the compiler's target service, including
OS-assisted RISC-V synchronization where supplied by the compiler runtime.

FP setters preserve unrelated hardware fields. x64 synchronizes x87 and SSE
rounding/enables and handles pending sticky exceptions without an unintended
waiting FLDCW trap. RV64 has sticky flags and rounding, but no hardware trap
enables: the API does not pretend otherwise. ARM64 exception enables depend
on hardware support; execution under QEMU is not a physical trap-support test.

Native64 a.out loading is explicitly unsupported before any file callback or
allocation. In particular, the former RV64 source must not accept x86 I_MAGIC
or apply 32-bit relocations to native pointers. No replacement object ABI is
invented. Existing supported 32-bit loaders keep their own magic/relocations.

Android uses the public unwinder for the current native64 C stack, not private
ARM32 ucontext offsets. This is not a signal-seeded ARM64/RV64 unwinder.
Directory rewind uses libc rewinddir; the obsolete private DIR-layout shim is
removed. Descriptor-limit conversion is checked and private, without libc
symbol interposition. Real startup/compatibility sources compile with modern
NDK for ARM32, ARM64 and x64. Runtime storage remains /sdcard/Inferno.

Modern compiler/linker fixes include sched_yield, single ownership of bflag
and exdebug, explicit Android prototypes/time declarations and the NanoSVG
comparator's actual Inferno qsort callback type. The comparator change is
scoped through NSVG_QSORT_ARGUMENT; standalone NanoSVG keeps const void*.
No Tk, security or crypto implementation is changed.

## Reproduction and evidence

Run from the repository root:

```
python3 utils/tests/host64-primitives.py
python3 utils/tests/host64-fp.py
python3 utils/tests/host64-cache.py
python3 utils/tests/android-fdlimit.py
python3 utils/tests/native64-backtrace.py
python3 utils/tests/native64-mkfiles.py
python3 utils/tests/host64-android.py --ndk /path/to/android-ndk-r28c
python3 utils/tests/native64-dynld.py --lib9 /path/to/Linux/amd64/lib/lib9.a
python3 utils/tests/mk-tempdir.py /absolute/path/to/source-built/mk
python3 libinterp/tests/draw-image.py
python3 emu/port/tests/startup-options.py
python3 libinterp/tests/jit-readiness.py --check
```

The primitive driver accepts --arch, --cc and --runner. Cross checks used NDK
r28c clang with --target=aarch64-linux-gnu or --target=riscv64-linux-gnu and
-fuse-ld=lld, then qemu-aarch64-static -cpu max or qemu-riscv64-static. These
freestanding Linux ELF tests use actual production assembly/FP code with a
small library declaration adapter, not a complete target libc or OS boot.
Each CPU executes 14,355 checks: 10,003 full products, 4,096 atomic exchanges
and 256 FP contexts. Independent native x64 fenv verification executes
557,056 transitions, thread isolation and a real SIGFPE. Native x64 cache
verification executes and rewrites W^X code on actual OS-sized pages.

Dynamic-loader checks link the full owning parser/error path and real lib9;
only allocation observation is bounded. Backtrace checks use a real host
unwinder with bounded declarations/logging. Android descriptor-limit tests
bound getrlimit/sysconf to exercise conversion extremes. The image decoder
oracle now uses active checks under NDEBUG rather than disabled assertions.

Classic mk builds were exercised in isolated checkouts. Configure Linux/x64
with utils/configure.py --host linux --arch amd64 --console, create output
bin/lib directories and run mk emuinstall with source-built mk/iyacc/decgen/
ndate on PATH and a writable TMPDIR. It links Linux/amd64/bin/emu-g as ELF64.
Attempting normal startup exits explicitly because legacy tables are not a
native64 registration. Help/option parsing precedes that refusal.

Android/ARM64 with NDK r28c builds headless core archives through mk emuinstall
CONF=emu-g CONFLIST=emu-g, using profile-console's EMUDIRS and the Android
mkfile as EMUMKFILE. This is an archive build, not a complete NativeActivity
link or an installable APK. The default GUI closure currently reaches an
unrelated libtk/window.c attachtowindow declaration failure; Tk is outside
the authorized implementation scope and was not patched. Rebuild archives
from clean sources after removing obsolete telldir.o; ar rcs alone retains
obsolete members from an existing archive.

## Remaining acceptance boundary

Installed builtin declarations, frame offsets, pointer maps, result/argument
slots and all module registrations must agree on int32/ref64 before a native64
VM can boot. Existing opt-in generated-header/binding/Math tests remain a
separate staged path. sizeof(WORD)==4 alone is insufficient when pointers are8.
Legacy startup therefore checks both widths before registering modules.

Next work is integrating that native64 registration across the required
builtin closure, then real module calls, GC/scheduler/error boundaries and
physical-target startup. Linux ARM64/RV64 full builds, modern Android GUI and
16KB-page device execution still need independent acceptance. A linked ELF64
or an isolated JIT test is not that acceptance.

Existing JIT dispatch coverage remains 173/173 including C handlers for all
three architectures. Native fast paths remain ARM64/x64 166/173 (95.95%) and
RV64M 103/173 (59.54%); RV64I87/173 and Zmmul90/173. These are opcode metrics,
not percentages of complete OS-port readiness. Dis64 file execution remains
the existing 43-op np0 scalar profile; managed-reference admission is unchanged.

The subsequent [explicit builtin constructor](dis64-native64-builtins.md) now
bridges selected trusted generated records to real runtime modules. It remains
opt-in and does not remove the full-startup legacy ABI refusal.
