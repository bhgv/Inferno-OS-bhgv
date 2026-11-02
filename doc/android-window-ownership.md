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
