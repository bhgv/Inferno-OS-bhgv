# Native64 series0104: bounded nonzero curve geometry and geometric storage growth

The prior curve checks deliberately covered allocation/zero geometry only.
Native Bezier/spline code still squared int coordinates, multiplied a segment by
an int dot product, scaled absolute Point fields in int and formed spline weighted
sums in int. Ordinary screen-sized bends could overflow; translating a small
curve near signed32 limits overflowed even when its relative geometry was small.
Source-origin subtraction could overflow before cancelling back to a valid value.

Fixed-point subdivision now uses a private BPoint with signed64 fields. Native
control inputs remain signed32, scale remains at most32768 before rescaling, and
convex intermediate magnitude stays below2^47. Every conversion back to native
Point checks signed32 range. Spline convex weighted averages widen before sums/
products and retain signed truncation semantics. No floating-point tolerance or
new external/native VM layout is introduced.

Point/segment distance retains exact integer projection/clamping in a bounded
relative domain: components at most2^20, squared sums at most2^43 and projection
products at most2^61. Larger relative spans conservatively subdivide rather than
perform overflowing products; coincident endpoints remain exact. This may emit
more vertices for huge straight spans than the mathematical minimum. All source
coordinate translation is checked in signed64 before a native draw side effect.
Unrepresentable spline source translations free the generated list and return0.

Each private Plist now owns its capacity and subdivision budget. Output is bounded
at65536 vertices (the existing one-polygon wire count), input spline count is
bounded likewise, work at1048576 admissions and path depth at64. Exhaustion frees
owned storage and returns the existing failure convention (-1/nil for the point
builder,0 for drawing). This is a deliberate native resource policy, not a shared
Dis64 field/feature or a promise that every finite curve succeeds. Actual display
buffers may still impose a smaller packet limit.

Storage grows geometrically32,64,...,65536 instead of reallocating after every32
vertices. At most12 native reservations serve a list. This bounds the total moved
vertex storage linearly when realloc must move; allocation refusal still retains
and retires the old owner exactly once. State/budgets are per-list, never global.
This does not establish concurrent VM payload/collector access.

native64-curves.py uses a separate Python arbitrary-precision legacy-order oracle:
exact integer distance/projection, fixed-point subdivision and spline averages.
No native widths or conservative C distance-domain fallback are copied into its
ordinary-domain reference. Nonzero/overshooting/closed curves and positive/negative
translations near signed32 boundaries compare every emitted vertex. Full-range
endpoint/convex-hull/property cases separately check conservative subdivision or
clean bounded refusal; they do not demand an identical tessellation above the
C exact-distance domain. Actual public cubic/spline outline/fill wrappers are
checked at an explicit native polygon transport, including source translation
cancellation and overflow refusal. This is not a device or general rendering
quality proof; no new Frame64 export-count claim follows.

Local x64 O3 nonrecovering UBSan with synthetic port aliases:86 completed exact/
property cases,1 clean bounded refusal,110306 predicates. Six actual-source
semantic10 controls remove work/depth/output/origin/projection/growth checks.
Exhausted bounds stop before unsafe allocation/execution; source controls never
accept a crash, compiler/sanitizer failure or timeout. The allocator also enforces
at most12 reservations per list, catching a restored linear-growth policy.

Existing owning polygons/codec/Frame64 gate passes191203 predicates/model and
all6 controls after changed allocation accounting. Reduced predicate count is
from fewer native growth/free events, not omitted packet/frame cases. Native
ARM64/x64 focused jobs now include this new oracle and full classic-mk runs it
for all target profiles. Their new results remain separate pending evidence.
Prior9d95a87 complete matrix has ARM64 and RV64/QEMU successes; x64 remained
running at the last checked checkpoint. No full-matrix success inferred.

Draw consumer-frame evidence remains40/89; these changes strengthen underlying
curve math rather than register additional VM exports. JIT fast/C populations,
public Dis64 admission43/173, shared IDs/layout and ordinary WORD32 LEA are
unchanged. FLASHVM-0102 now explicitly agrees to a distinct opt-in Inferno launch policy;
that policy is not yet activated by this graphics batch. Android/AWM, managed graphics lifetimes and ordinary loader activation
are still separate milestones.


## FLASHVM-0102 staged-policy agreement received

Synced incomingd3682221bc4bff85d61a6e54e7bdb8f30dd1dbc4 and read the explicit
reply plus pinned9a58806a18ad3c6790729f741cdf82467c816180 FILE-CALL-GOLDENS.md.
FlashVM agrees that its not-yet-implemented independent typed validator/executor
need not block a distinct Inferno opt-in experimental path for the reviewed
Int32/Big/Real root-to-leaf subset. Ordinary/default/shared admission remains
unchanged. Unsupported input must fail without fallback, every module still needs
raw proof/owned construction, mode is not authority, and target evidence remains
per backend. No Bool/managed/imports/nesting/new IDs/native pointers are approved.
This resolves the previous policy question; no launch implementation or acceptance
is claimed in0104.

The next entry-path work must bind an explicit startup profile to constructor,
initial module selection and cache acceptance. A cache hit must not cross between
ordinary scalar and result-address cstep modes, even when path/qid match. File
read errors, default43/173 admission, unsupported legacy/dynamic inputs, source
independence and protected frame/module/code retirement must be tested through
actual readmod/parsemod/scheduler, with no implicit broader fallback. Existing
signature/admission policy is preserved. The new option should be fixed before
VM startup rather than mutated around concurrent loads. A new shared wire bit is
not part of this agreed host-side opt-in policy.

## Native ARM64/x64 acceptance

Source10025578a4d748669b94f1d6945ac4145ef7aef3 passes focused jobs126045479591
(ARM64) and126045480047 (x64) in41672843771. Both actual logs were read:
86 completed exact/property cases,1 clean bounded refusal,110306 curve predicates
and all6 work/depth/points/origin/projection/growth semantic controls. Both targets
also pass191203 polygon predicates/model with all6 codec/ownership controls and
8 direct/file vmachine cases under UBSan. This is physical-target acceptance of
real native curve arithmetic/allocation and bounded transports; complete workflow/
classic-mk/RV64-QEMU results remain separate pending evidence. No device/AWM,
additional Draw export registration or experimental loader activation inferred.

Peer4eae14d51c3553d6aa699fb668aea92bd29086ef explicitly confirms cache-profile
separation and startup-only mode binding. That incoming journal is preserved in
the evidence publication base; no peer guard/parallel runtime code is imported.
