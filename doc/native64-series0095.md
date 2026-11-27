# Native64 series 0095: complete generated Draw geometry boundary

The complete owning Draw interface could not be generated because the public
Draw_Rect_size callable collided with the Draw_Rect size metadata macro. The
metadata generator now reserves callable names before descriptor macros and
moves only colliding size/map macros into its reserved private namespace.
Actual callable names, signatures, frames, type layouts and map8 stay unchanged.
Real flattened type/constant collisions still refuse before publishing output.
The change also handles cross-module callable names that shadow frame metadata.

module/draw-native64.m includes the actual Sys and Draw source interfaces.
Fresh Limbo emits all 89 fixed Draw specifications. The complete owning geom.c
can use that generated int32/ref64 interface through the explicit
DIS_NATIVE64_GEOMETRY_HEADER opt-in; it requires DIS_WORD32=1. All 19 Point/Rect
methods use their qualified generated functions and Frame64 declarations.
Default legacy consumer selection and geometry arithmetic bodies are unchanged.
This does not register the other Draw exports or activate a graphic device.

The new native64-geometry.py builds the real consumer as a separate translation
unit. Independent Python integer oracles cover all 19 functions, boundary and
seeded random coordinates, modular arithmetic, zero divide, half-open bounds,
clipping, complete tuple results and every byte offset of partial return/input
aliasing. Surrounding bytes are checked. Predicates remain active under NDEBUG.
Two consumer-only mutations must stop at semantic exit 10: shifted input and
truncated result; compiler errors, signals and sanitizer failures do not count.
Missing/zero WORD32 consumer selections are rejected at compilation.
Synthetic type size/map and cross-module frame size/map collisions are compiled
and invoked through actual generated entries/tables. Genuine ambiguous names
must still reject before any output.

Local x64 O3 nonrecovering UBSan passes 263784 predicates per WORD32 and
WORD32-file model, both semantic controls and the synthetic collision bindings.
Fresh Sys, Math and selected Freetype headers are byte-identical to series0094
(122712, 170263 and 27841 bytes). ARM64/x64 ASan and complete ARM64/RV64 port
runs are CI gates; their results must be read separately once published.

This is a C consumer ABI step, not a new JIT opcode or a wire format change.
Native fast paths remain ARM64/x64 166/173 (95.95%), RV64M 103/173 (59.54%);
JIT plus C coverage remains 173/173, distinct from file scalar admission43/173.
A complete Android AWM startup still needs the remaining Draw resource/device
consumers and verified managed call/import, scheduler and GC boundaries. The
existing ARM64 scalar APK does not establish that full graphical readiness.

FlashVM communication remains in the two repository journals. FLASHVM-0092 arrived during publication and was preserved. Its pinned
POINTER-BUILDER.md was reviewed as a private hosted contract, without importing
or claiming an owning replay. No shared Dis64 ID,
layout, native-pointer encoding or file CALL/LEA8 activation is proposed here.

The preceding complete classic-mk port workflow41611046060 has now finished
successfully on all three jobs: native ARM64, native x64 and RV64/QEMU.
That is series0094 acceptance; it is not a substitute for the new0095 jobs.

Local complete native64-layout and all72 real-source Dis/AOT compiler cases
also pass with the fresh compiler.
