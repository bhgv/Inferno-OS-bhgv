# Series0106: checked primitive packets, signed borders and15 Draw Frame64 consumers

The complete ARM64 scalar console now reaches the named Dis64 call profile through
real file loading/startup/vmachine; see the prior-series receipt below. This batch
addresses the next graphics-consumer prerequisite without activating managed Draw
registration, importing FlashVM runtime code or widening shared bytecode admission.

## Native fixes

Rectangle draw/gendraw, line and ellipse/arc command builders now use one private
reservation for an optional operator prefix plus its45-byte command. Previously
_setdrawop could commit an operator and the subsequent command reservation could
fail, leaving that operator for another draw. A single refused reservation now
emits neither. Existing wire bytes and command order are unchanged on success.
The private draw-packet.h is listed in classic-mk header dependencies. This is
buffer-reservation atomicity, not concurrency/locking or all-library transaction
support: other legacy command families still use their existing operator helper.

These native families reject nil images/displays, cross-display resources,
unavailable default source/mask, invalid composition values and negative line/
ellipse radii before packet reservation. Empty rectangles remain representable;
inverted rectangles refuse. Default native source/mask remain black/opaque; the
VM consumer's existing H-mask default remains white. The tests deliberately use
three different native default-image identities to catch accidental conflation.

Arc bit31 is formed with u32int, never signed `1<<31`; start/sweep bits retain
existing wire representation. Border rectangle expansion, source-origin offsets,
width negation and edge calculations use signed64 intermediates, including
INT_MIN widths and full signed32 endpoint differences. Every edge/origin is
checked before narrowing or the first draw. Unrepresentable border geometry
refuses as a whole. Existing per-command buffer refusal is retained: a later
border command can still fail after earlier commands have been emitted.

## Independent owning evidence

native64-primitives.py links actual draw/line/ellipse/border/geometry implementations
and15 actual libinterp Draw consumers with fresh compiler-generated Frame64
structs. An independent Python unlimited-integer oracle supplies3000 native/VM
cases and exact little-endian packet bytes: full signed32 endpoints, near-limit
translations, positive/negative/INT_MIN borders, outlined/filled ellipses/arcs,
explicit/default operators, independent source/mask origins and default-image
selection, cross-display and malformed dimensions. Every complete reservation is
individually refused as well; bytes, guards, request counts, argument preservation
and balanced display exclusion remain checked under NDEBUG.

Local x64 O3 nonrecovering UBSan passes2209253 predicates per WORD32 and
WORD32-file model, including synthetic port aliases. Seven actual-source controls
restore split reservation, remove border narrowing, drop arc flag, permit a
foreign line display/negative radius, swap a default image or exchange VM origins.
Each compiles then stops at semantic10; a crash, compiler/sanitizer error or timeout
is not a successful control. Native lookup/locks/I-O are explicitly bounded test
transports, not resource/GC ownership, device or final rendering proofs.

The changed native library also rebuilds via classic mk in the existing coherent
x64 console tree; full emulator file-call boot still passes11 successful roots
plus two leaf-error roots. Native ARM64/x64 ASan/UBSan jobs and the complete
classic-mk ARM64/x64/RV64 matrix now include the new primitive gate. New target
acceptance is separate pending evidence until actual logs are read.

Bounded Draw consumer-frame evidence grows40/89 to55/89:19 geometry,11 scalar,
2 pixel,8 Point-array and15 primitive consumers. This is not55 registered managed
exports or a full AWM/Android readiness percentage. Native fast paths remain
ARM64/x64166/173 (95.95%), RV64M103/173 (59.54%), with173/173 JIT+C entries.
Default Dis64 scalar admission stays43/173; the named scalar-call policy remains
separate. No shared encoding/ID/layout or ordinary WORD32 LEA change follows.

## Prior-series complete target receipt

Read actual source05c1bda logs in41680117387. Native ARM64 job126071226103
and RV64/QEMU job126071226109 both completed successfully, including call-boot,
40-image file-calls and all3 controls, complete classic-mk boot/lease gates and
128 live-local full-emulator processes/two wrong-answer controls. This is actual
ARM64 full-console file-call startup, beyond its previously reported focused
ASan diagnostic. The x64 job126071226372 was still running at this checkpoint;
no full-matrix success is inferred. Native ARM64/x64 focused source05c1bda jobs
already passed and remain separate from this new primitive source evidence.

FlashVM's incoming journal remains FLASHVM-0104/e228337 at the initial check.
It independently reproduced the previous40-image x64 file route and3 controls;
its independent typed-call validator/executor remain planned with explicit refusal.
This series requires no format extension or user relay, and imports no hosted
cell-domain/guard/MOVP/multithreading implementation.
