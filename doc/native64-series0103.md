# Native64 series0103: checked Point arrays, polygon encoding and curve-list retirement

Draw's polygon/spline consumers dereferenced p->data/len without checking a nil
array, array owner, element geometry or storage extent. Eight real entry points
now share a captured Point view before image lookup or display exclusion. The
view requires actual Tarray owner identity, native-compatible Point size with no
pointer map, count0..65536, aligned nonempty backing storage and bounded extent.
Empty storage becomes nil without converting an invalid empty data pointer.
These are metadata/extent checks under caller-owned VM roots/exclusion, not
arbitrary host-pointer provenance, a new retain or moving-GC lifetime pin.

Native polygon encoding now bounds np to the existing16-bit np-1 wire field,
rejects nil/cross-display/wrapped inputs, and bounds temporary allocation. Delta
arithmetic uses a wide signed difference. Signed7 deltas remain exact; an absolute
coordinate must fit the existing signed23 representation. Negative absolute bits
use unsigned shifts. A small delta crossing the absolute boundary remains legal.
Unencodable input releases temporary storage before compositing/packet effects;
it no longer silently truncates extreme coordinates. Existing field layout and
four native polygon APIs remain unchanged. Real displays may impose a smaller
buffer limit than the wire count; normal buffer refusal is still handled.

Curve-list growth now retains the old pointer until realloc succeeds. On failure
it releases the old owner, clears the pointer and marks the list failed. All five
owning Plist constructions initialize the pointer. Recursive calls and segment
iteration stop after allocation refusal; growth arithmetic is bounded before
multiplication. Existing curve geometry/arithmetic otherwise remains unchanged.
This does NOT prove arbitrary large/nonzero curve geometry safe: legacy distance/
fixed-point arithmetic and total tessellation work remain separate audit targets.

native64-polygons.py generates an independent Python unbounded-integer coordinate
oracle. Actual four polygon encoders, curve/list/arith source and eight VM consumer
bodies run with fresh compiler-generated Frame64 declarations.128 boundary/random
polygons, default/explicit compositing, outline/fill, maximum65536 wire count,
negative/over-limit counts, nil/foreign/wrapped inputs, allocator and buffer refusal
are checked. Native protocol headers and every compressed byte are compared.
Zero-coordinate real spline/Bezier positives exercise all list realloc refusals
and four native Bezier wrappers. Fifteen array/lookup shapes per VM entry cover
nil/type/map/count/alignment/wrap/empty/cross-display/radius behavior before effects.
There is no device namespace, image ownership, scheduler lock or arbitrary-curve
geometry proof in these bounded transports.

Local O3 x64 UBSan with synthetic port aliases passes189425 predicates per WORD32
and WORD32-file model. Five actual-source controls damage packet identity, remove
wire count/absolute range/Point-type validation or omit old-list retirement; each
requires semantic10 before unsafe continuation. The private test allocation ledger
cleans failed-control storage only for sanitizer exit hygiene, not production
rollback. Existing complete Draw scalar gates pass340506/model and original
pixel transfers pass96424/model with their controls. No Tk source was changed.

Bounded Draw consumer-frame evidence now covers40/89 exports:19 geometry,
11 scalar,2 pixels,8 Point-array consumers. This is not complete Draw registration,
managed image/font/display resource lifecycle or Android/AWM readiness. Native
ARM64/x64 polygon ASan/UBSan and full classic-mk now include the new test.

## Prior-series regression evidence and closure repair

All six native jobs in41661075217 for6c11b08 passed, including the repaired
whole-image controls, inference and owning constructors. Full41661074661 instead
exposed an undefined native64lea8step reference in the separately assembled
scheduler/vmachine TEST source closure on ARM64/RV64. Production constructor code
was present, but this older diagnostic closure had not included its new callback
dependency. It now borrows the actual xec helper, without a mock implementation.
Local x64 direct/file pilot42/init42/divide-zero/shift-invalid cases all pass after
the repair. Focused native polygon jobs also compile/execute that closure with
UBSan, so the same missing dependency is caught early. Full new-source matrix
acceptance remains separate pending evidence; failed old jobs are not counted.

## Dis64 coordination

Synced incoming FLASHVM-0100 atea3640c8f93b1272387bc1fbcb060afa9baab7dd.
Read pinned71b93033875ca69a25491937b28e159cd2f6c4a6 FILE-CALL-GOLDENS.md.
Peer reproduced raw inference, actual constructor and repaired whole-image tests
on x64, plus exact new Type-order image identity/refusal. That is reproducibility
and outer framing, not an independent typed-call validator/FlashVM execution.
Public43/173 and shared FRAME/CALL/RET/LEA admission remain unchanged. No new IDs,
wire/native layout, owner-JIT transplant or parallel managed execution introduced.


## Native phase-one acceptance and paired decoder follow-up

Source0319eaa3567ff5827ce116ce7f0470a5e676141d passes focused native ARM64/x64
jobs126021442955/126021442738 in41666025411. Both actual logs were read:
189425 polygon predicates per WORD32/WORD32-file, all5 packet/count/absolute/
array/retire controls, plus all8 direct/file vmachine input cases under UBSan.
These are focused jobs, not entire-workflow/full-matrix completion.

Reviewing the paired server decoder found negative left shifts in sign extension
and an avoidable p+1 check beyond a truncated span. drawcoord now reconstructs
signed7/signed23 values by bounded subtraction, checks remaining bytes with a
same-span difference and rejects relative signed32 overflow using a wide sum.
Output publishes only after validation. This preserves existing draw protocol
bytes and valid relative coordinates; malformed input uses existing Eshortdraw/
Ebadarg channel error handling. No Tk or security implementation is changed.

The expanded fixture links this actual server decoder. It round-trips native
packets against the independent original point values, checks all absolute
truncations, signed endpoints and both relative overflows with unchanged refused
output. Local x64 O3 UBSan with port aliases passes192177 predicates per model
and a SIXTH strict decode-sign control, in addition to the five prior controls.
Native target acceptance for this follow-up must be recorded separately; the
phase-one189425 result is not silently attributed to the decoder change.

FLASHVM-0101 was received at24199a309051390fc6061d09f8c181a0126e6a61. Its
Draw-scope/lifetime restrictions are acknowledged. The outgoing journal proposes,
without enabling, distinguishing fixed shared wire semantics from per-VM opt-in
executable subsets so loader progress need not imply immediate FlashVM execution.
A concrete peer policy reply is requested before changing current admission.
