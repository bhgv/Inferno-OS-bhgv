# Android ARM32 multi-window follow-up

The phone photograph reports `wmclient: tk error window path already exists`
while the debugger opens additional windows. Native Tk.toplevel returned the
process's existing top, so each caller attempted to recreate `.Wm_t` and its
children in the same namespace. It now constructs a fresh top for every call.
The first live top remains the primary #G root; secondary window destruction
cannot detach it. Its weak owner is resolved by PID so later collection cannot
dereference a retired Prog.

Draw.Display.allocate also returned the cached display without retaining it.
The corrected default-display path retains before replacing an aliased result;
explicit device paths still allocate their requested display. This fixes a
concrete ownership error that can affect both framebuffer and GLES consumers.
The screenshot alone does not identify every earlier Bounce or font symptom.

The ownership fixture passes14160 cases per WORD64/WORD32 model under UBSan
with NDEBUG. Restoring either old fault triggers an active WORD32 control.
Tk/Draw layout records and real production function bodies are used; native
widget/context/display plumbing is bounded and does not establish phone GUI
acceptance. The APK build compiles and links the actual ARM32 core separately.

FB's existing identity pixel/touch transform remains in place. GLES retains
its own uniform canvas transform, sequence-based touch translation, immutable
window-buffer geometry, UI-thread ownership and private Inferno FreeType symbols.
The host geometry/input, lifecycle, storage, clipping and FreeType gates are
repeated before packaging. No application Java/DEX or private runtime tree is
introduced. Existing /sdcard/Inferno contents remain reusable unchanged.


## Verified ARM32 APKs, 2026-11-02

[Build41374740439](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41374740439)
passes both renderers. Core pin:f9ea7ac0d47bfe16573fd77f99ca9c5e2f76d5da;
FB launcher:638710fd24be56ef8aee2bade350a941f8eb6990;
GLES launcher:284c5ecc837a9fec292a136173d23d7a77194fcc.

| Renderer | Signed APK and build evidence | APK SHA256 |
| --- | --- | --- |
| FB | [Download](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41374740439/artifacts/13111125768) | 4df36635fdae1a440cc705c6c463bda4070219fe47983ab147753c18c8457ac8 |
| GLES/Clutter | [Download](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41374740439/artifacts/13110612907) | 400e0f7c55952e6aa08414212d82829d8041e28a27c36111a7510a6e01a58ac6 |

The downloaded APKs match their hashes, contain actual ELF32/ARM native
libraries, no DEX and the required AWM runtime. CI verifies APK v1/v2 signatures
and ZIP alignment. Application IDs end in windowfix20261102 and versionName is
windowfix-2026-11-02; open the newly installed dated test application.

Each renderer's build passes14160 ownership cases in each host WORD64/WORD32
model under UBSan/NDEBUG. Separate static Bionic/API17 ARM32 fixtures run33
constructor/display cases under qemu-arm, with both old-fault controls active.
These are actual ARM32 pointer layouts with bounded native window adapters,
not execution of an Android GUI. Explicit Android headers and API17 are selected
for these cross fixtures; native adapter allocation uses libc consistently.

FB pixel/lifecycle/startup/storage gates pass. GLES input/geometry checks3312
predicates; startup/UI-thread/clip/storage gates pass. Both private and renderer
FT_Init_FreeType implementations are defined separately in the downloaded GLES
APK (inferno_private_FT_Init_FreeType versus FT_Init_FreeType), matching the
host private-FreeType link control. This preserves renderer-specific transforms.

Repeat the photographed debugger additional-window scenario and Bounce on the
phone. Physical rendering, font appearance and all device-specific crash paths
are not established by host/QEMU ownership fixtures. Existing /sdcard/Inferno
files remain reusable; no private runtime tree or extraction migration is needed.
GitHub artifacts require login and follow14-day repository retention.
