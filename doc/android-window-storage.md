# Android GLES window storage and Activity lifetime

The November 1 phone videos show a horizontally compressed GLES menu, abnormal
Bounce rendering and Android crash dialogs for both clients. Videos do not supply
native program counters, so a particular crash cannot be attributed conclusively
to one source defect from these recordings alone.

A reproducible GLES defect exists in `libmemlayer/lalloc.c`: EXT_WIN allocates
an independent window image, then overwrites its row stride and coordinate origin
with those of the full screen and adds an extra data reference. Standard memory
drawing paths can subsequently address beyond the window allocation, and freeing
the window leaks its image data. Keep the independently allocated geometry and
reference count; only software screen views inherit the parent screen geometry.
Allocation failures now release acquired image/actor resources in both modes.

`libmemlayer/tests/android-window-storage.py` compiles the actual allocator for
both modes. Its independent 127x400 window at (317,201) checks every pixel through
the last row against the allocated span, reference ownership and failure cleanup.
The old EXT_WIN source fails this control. This is a bounded geometry/ownership
fixture, not an Android display or full Bounce execution test.

The Clutter launcher additionally clips actors without reallocating their
borrowed source geometry, and texture uploads no longer negotiate logical actor
size. FB retains its original one-to-one display and touch coordinates. Both
launchers handle configuration changes without needless Activity recreation.
When a running VM's Activity really terminates, its process exits so VM workers
cannot survive with an invalid native Activity/stack-owned UI context. Returning
from NativeActivity's native main alone does not stop those workers.

Fresh APKs must still be tested on the phone for exact font appearance, Bounce
and repeated foreground/background transitions. If a crash remains, preserve
`/sdcard/Inferno/startup.log`, `/sdcard/Inferno/backtrace.log` and Android's native
crash log; their evidence is distinct from the videos and host regression tests.
No Tk or cryptographic source changes are made for this series.
