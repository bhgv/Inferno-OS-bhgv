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
