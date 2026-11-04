# Native64 series0121: staged Display allocation and explicit construction roots

2026-11-04, experiment, ARM64 first. This series fixes the actual Display.allocate
C consumer and its private managed/native graph construction. It does not enable
managed Draw, widen Dis64 admission or change Android/Tk/crypto/security sources.

## Production changes

Display.allocate previously destroyed its old result before converting the device
String, so a reused input/result cell could become stale. It now captures input
and possibly unaligned result bytes, prepares the result, publishes it, then
retires the displaced owner. Nil/H/wrapping result addresses and a nil old owner
refuse before resource allocation. The unchanged cached-display helper operates
on an aligned local, keeping its existing reference contract even for packed
caller results.

The native display lease is allocated before opening a display. Managed owner,
pin and child-wrapper failures now release the lock and return the full partial
graph to baseline; the previous DRef-allocation failure leaked the opened display.
Each default image uses its own geometry/channel instead of reusing white's
rectangle for all constants. Existing optional default-font/build/cache policy is
preserved and runs after fallible wrapper construction.

The unpublished parent is pinned through actual ptradd before nested managed
allocation. Each child transfers into its actual mapped parent field and Setmark
publishes that edge before another allocation. Mutable build state lives outside
kprotect's error frame. The pin survives native font preparation, unlock, caller
publication and old-owner retirement, then ptrdel removes this pin only.

Rollback first unlocks, clears parent edges and returns native ownership from
staged wrappers before retiring their managed backedges. The private parent's
native pointer is detached so its destructor does not erase still-owned native
default aliases; final DRef retirement closes them. Root-allocation refusal and
managed allocation errors preserve the original diagnostic across cleanup.

Initial ptradd metadata allocation retains its existing no-yield/no-collection
contract for an unpinned fresh owner. Caller frame/input/result lifetime and
nonthrowing destructors remain required. This is not a concurrent GC/lifetime
protocol, remote rollback or an arbitrary native callback-unwind proof.

## Verification

39 actual Frame64 scenarios/16,033 active predicates pass in each WORD32 and
WORD32-file O3 nonrecovering UBSan run. Four malformed-result checks pass too.
The fixture compiles actual Heap/Type/maps/protected allocation/retirement,
cached-display helper, new constructor/consumer and gc.c ptradd/ptrdel. External
root bookkeeping and mapped child reachability are checked at nested allocation
boundaries. No fabricated Heap or native-key roots are used.

Seven exact-stage omissions pass: input lifetime, early publication, native image
ownership, private display detachment, unlock, construction root and mapped edges.
Each stops before unsafe continuation; compiler/resource/sanitizer/signal/timeout
failures cannot count. Native init/close/font/cache/lock/String/device services are
bounded adapters. Real collector traversal is not run; successful cyclic graphs
are explicitly broken by test teardown.

Complete owning Draw compilation and the prior scalar gate pass340,506 predicates
per WORD32/file model plus its result control. Registration initialization retains
its early native64 refusal and23 descriptor cases/model. Classic mk builds the
actual draw.o successfully. Focused native ARM64/x64 ASan+UBSan jobs and the full
native64 pipeline include the new fixture and seven controls; source-bound hosted
results are pending at publication. No full-matrix/RV64/Android acceptance follows.

## Coordination and readiness

Read incoming FLASHVM-0121/0122 on preserved history through2972714. Native guarded
string/REAL work remains peer-reported; its two-phase FP boundary is not imported
as a shared Frame64 contract. The INFERNO-0120 request for independent typed-role/
function-ownership, initialization/result/LEA checking on the same immutable25
images remains pending. No user relay or native multithreading import is needed.

Bounded Draw C-consumer evidence grows84/89 to85/89. This counts one additional
consumer fixture, not85 registered exports or full display/font/collector/GUI
acceptance. Managed registration remains closed. JIT fast counts stay ARM64/x64
166/173 and RV64M103/173, with C support173/173. Default Dis64 numeric43/173,
ordinary WORD32 LEA and immutable named dis64-calls launch/cache policy are
unchanged; no shared wire/feature/ABI IDs are added.

Console ARM64 already has prior build/execution evidence. Full managed imports/
calls, integrated roots/GC/resource/refresh lifetime and Android/AWM execution
remain substantial work. No APK or reliable completion date is claimed.

## Exact-source native target receipt and remaining consumer inventory

Sourcee30449ba96ae30f6c0cf199ad62ec1bd2e7f8161, workflow41789369934:
ARM64 job126438896376 and x64 job126438896242 both completed successfully.
Actual logs were read:16,033 active checks/39 Frame64 scenarios in each WORD32
and WORD32-file profile, all seven exact-stage controls, and23 descriptor cases
per initialization model under O3 ASan+UBSan. Ordinary leak detection remains
enabled. Pending status is superseded for these focused jobs only; full-matrix,
physical RV64, collector/renderer/managed CALL and Android acceptance do not follow.

A fresh Limbo header exposes89 Draw entries. Comparing the existing fixture name
sets with those declarations gives85 bounded consumer entries and four remaining:
Image.bezier, Image.bezierop, Image.fillbezier, Image.fillbezierop. Their underlying
native curve algorithm has separate prior evidence; these four Frame64 bindings
still need their own argument/mode/alias/refusal checks. This inventory does not
reduce the remaining integrated root/resource/collector/refresh/import work to
four tasks or establish whole-GUI readiness.
