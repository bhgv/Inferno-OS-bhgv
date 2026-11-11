# Android ARM64 full GUI native build

The ordinary Android framebuffer GUI now compiles and links with NDK r27c as
ELF64/AArch64 using WORD32/ref64. This is the complete Draw/Tk/FreeType/audio
NativeActivity graph, rather than the separate builtin-free scalar APK probe.
It is not yet a usable AWM APK: normal startup still rejects legacy builtin
registration on LP64, and general managed Dis64 compiler/loader admission remains
unfinished. No ABI guard has been disabled.

Use core `experiment` and companion `Inferno-OS_Android` branch
`experiment-arm64-gui`, commit `ed8b80ecb64524c86ca1bd2b21e1abbfb494473f`.
Bootstrap host tools with `makemk.sh`, then run:

```sh
python3 utils/tests/android-gui-build.py \
  --ndk /path/to/android-ndk-r27c \
  --host-tools /path/to/host-tools \
  --app-root /path/to/Inferno-OS_Android \
  --build-root /path/to/empty-output
```

The output must be outside both source checkouts and initially empty. The driver
copies current tracked inputs, selects one coherent WORD32 model for every core
object, builds `emu` including GUI devices, builds the remaining application
libraries and links the actual NativeActivity. CMake/Ninja and the modern NDK are
required. Flexible 16KiB page support is selected for the final shared library.
`logs/`, ELF symbol/segment inspection and `result.json` preserve build evidence.
The result explicitly records `gui_native_linked: true` and `awm_started: false`.
It does not package or label this blocked runtime as a working desktop APK.

Fixed real build failures include the audio callback's `int*` declaration,
FreeType inheriting the emulator's header prerequisites, the missing
`attachtowindow` prototype, and duplicate Tk/font/compiler-mode global owners.
The companion frontend uses the supported `ALooper_pollOnce` API and correctly
typed sample-buffer pointers. Android defaults remain stylus input and interpreter
mode, initialized by `amain` before option parsing.

NDK r27c builds actual startup/compatibility objects for ARM32, ARM64 and x64.
Framebuffer lifecycle, startup allocation, pixel layout and three-way CMake ABI
selection tests pass. The native Screen-channel test passes 7245 checks across
188 cases. Native linking does not replace executing AWM on Android or checking
its window, input, storage permissions and JIT policy on a device.

The next startup work is coherent native64 C declarations, module registration,
frame/maps and general managed compiler/runtime support for the AWM dependency
closure. A scalar module's success or a linked GUI library is insufficient proof.

## Native64 Tk registration step

Add `--native64-tk --native64-draw` to compile the whole GUI graph with generated Frame64
Tk declarations and its real `builtinmod64` registration path. Classic mk
regenerates `tk-native64.h` from the actual Sys/Draw/Tk `.m` dependency closure;
only the Tk call table is emitted. The `TkTop` public prefix is checked against
the generated `Tk_Toplevel` field offsets and extent at compile time.

The new initializer stages its descriptor before registration, publishes no
partial top on failure, preserves the registration diagnostic and installs the
formatter/widget table only after success. Repeated initialization is
idempotent. Header-only selection and an implicit WORD model remain refused.
The legacy initializer retains its existing order.

`python3 utils/tests/native64-tk-register.py --limbo /path/to/limbo` compiles
all actual Tk callback bodies and exercises the real module/descriptor
constructor and initializer with controlled allocation/error boundaries. It
checks all 10 generated links, maps, callback identities, the public-prefix
geometry, idempotence, 26 allocation refusals and 378 staging observations.
The GUI callback witnesses abort if called: this registration test deliberately
does not claim GUI operation. Host UBSan and ARM64/Bionic QEMU runs pass;
portable compiler/runner options use `CC`, `NATIVE64_ARCH`, `NATIVE64_CFLAGS`
and `NATIVE64_RUNNER`, as with the other native64 gates.

The full GUI shared library also links with this selection and retains 16KiB
LOAD alignment. `result.json` records `tk_native64_selected: true` while
`awm_started` remains false. Ordinary Android startup still refuses the mixed
legacy builtin set. The full Sys variadic/import contract and general
managed Dis64 compilation remain startup work; this flag does not bypass that
boundary or produce a working AWM APK.

## Coherent Draw, geometry and devgui frames

The Draw owner now registers all 89 fixed exports through generated native64
metadata, retaining staging and rollback for its four native wrappers. Geometry
methods in `geom.c` select the same Draw declarations. The `devgui` C caller
uses one generated Sys/Draw/Tk declaration set for its direct API calls, instead
of silently constructing old frames for new callbacks. The GUI driver requires
Draw and Tk together; either flag alone is refused before creating output.

`native64-draw-register.py` compiles both complete owners and validates every
registered name/hash/frame/map/callback, four wrapper descriptors, idempotence,
187 allocation refusals and 17766 staging observations. It links the actual
`geom.c` and invokes 301 published geometry callbacks, checking modular
arithmetic, zero-divide without result publication, unaligned and aliased Point
results, and the tuple returned by Rect.clip. Other window/device callbacks
are identity witnesses that abort if executed; no window presentation is claimed.

The complete Android NativeActivity links with both new GUI interfaces and
16KiB LOAD alignment. Host UBSan, ARM64, RV64 and ARM64/Bionic registration
checks are separate from running AWM bytecode. `result.json` records both
selected interfaces and still records `awm_started: false`. Ordinary startup
continues to reject the remaining legacy module set. This step changes trusted
C metadata and consumers only; it assigns no new Dis64 wire IDs or import ABI.

## Complete fixed-argument Sys set

Use `--native64-tk --native64-draw --native64-sys-fixed` to build the full GUI
native graph with all 39 fixed Sys exports, beyond the separate 15-entry console
pilot. Both Sys C owners and the service/channel consumer select the same fresh
`sys-native64.h`. The normal Android startup ABI guard remains active.

The Sys initializer allocates all five descriptors before asking the real
`builtinmod64` constructor to publish the complete module. Descriptor globals
and channel maps are published only after registration succeeds, with no later
allocation. Failed registration retires the staged descriptors while preserving
the diagnostic. Repeated initialization verifies the complete module and its
maps; incompatible preexisting modules or partial descriptor sets are refused
without claiming their owners. The original legacy and console pilot paths are
unchanged.

`native64-sys-fixed.py` compiles both complete Sys owners, tests all 39 generated
links/frames/maps and checks the actual descriptor/module initializers. It covers
88 allocation refusals, 32 preexisting-state refusals and 4005 staging observations.
Sixty calls through published links execute the production Unicode handlers,
including tuple/unaligned results, invalid runes and bounds-error nonpublication.
Other callbacks are aborting identity witnesses, so this fixture does not claim
OS I/O or GUI execution. Host UBSan, ARM64, RV64 and ARM64/Bionic checks pass.

The selected NativeActivity also links with 16KiB LOAD alignment. Its result
records `sys_fixed_native64_selected: true`, alongside Draw/Tk selection and
`awm_started: false`. This fixed set intentionally contains no print/fprint/sprint/
aprint entries. Their typed C gateways are separate from the unimplemented
ordinary variadic frame-tail/compiler contract. No shared Dis64 tags or general
managed file-call admission are added by C registration.

## ARM64/Bionic managed execution gate

The Android workflow also builds and executes the bounded managed String/List
runtime fixture against the NDK's real Bionic headers and static libc. It runs
under `qemu-aarch64`; a GNU ARM64 sysroot is not used for this gate:

```sh
NATIVE64_RUNNER=qemu-aarch64 python3 utils/tests/dis64-numeric-gc.py \
  --managed --limbo /path/to/host-tools/limbo \
  --android-ndk /path/to/android-ndk-r27c
```

The explicit option selects ARM64 NDK clang, Android port headers and a static
Bionic executable. Compiler temporaries remain within the fixture's writable
work directory. The fixture honors Inferno's private Android `free_` name at its
tracked allocation boundary and uses the Android header's inline `getcallerpc`.
The Linux build and allocation hooks retain their existing selection.

This gate executes the real managed loader, collector, bytecode owners and JIT,
including 96 nil/alias/file-reload/stack-growth cases and 324 native completions.
It checks exact active JIT mapping bounds and complete root/module retirement.
The workflow preserves `bionic-managed.log` with the native build artifacts.
QEMU/Bionic execution is separate from AWM's remaining general compiler and
variadic ABI work, and from GUI/input/storage testing on an Android device.

## Typed Sys C gateways in the full GUI library

Add `--native64-formatter` together with `--native64-tk --native64-draw
--native64-sys-fixed` to compile and link the existing four typed C gateways.
The driver checks that `Sys_print64`, `Sys_fprint64`, `Sys_sprint64` and
`Sys_aprint64` are defined dynamic symbols in the resulting ARM64 library,
alongside the NativeActivity entries and 16KiB LOAD alignment. It records
`typed_sys_formatter_linked: true` and still records `awm_started: false`.
The option is refused before output creation if the fixed Sys profile is absent.

The Android workflow executes the complete typed formatter/argument/result/
write-ownership suite with static Bionic under QEMU, preserving
`bionic-sys-format.log`, then builds this full GUI configuration. Its non-callable
variadic prefix schemas come from the actual `sys.m`; all 39 fixed Sys exports
retain their separate registration path. This does not provide a compiler or
bytecode producer for typed variadic tails and does not enable ordinary AWM startup.
