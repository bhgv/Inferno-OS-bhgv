# Native64 series0123: complete native display-construction rollback

2026-11-04, experiment. Following the accepted0122 cubic-consumer/independent
Dis64 checkpoint, this series repairs lower-level native display initialization.
It does not change a JIT backend, managed Draw registration or the shared format.

## Production repair

initdisplay's old labels lost Display/Image allocations and successful retained
channels when a later fd-to-channel conversion failed. Failed default-image
construction freed only some image headers directly and could abandon the initial
lock and other defaults. Device-directory copying was unchecked. Cleanup could
erase the initiating diagnostic and leave only a later release error.

All local owners now start empty. The Display is zeroed immediately after its
allocation, before another allocation can fail. Independent device/window copies,
three raw descriptors, three retained channels, root header, four default images
and native lock have one refusal path. Partial images retire while exclusion/
channels remain live; exclusion is then released before lock retirement. Raw
FDs and channel retains each retire once. Success adopts every owner and preserves
the established initially-held-lock API, including local displays.

Channel/default-image construction stops at its first refusal. Wrong information
lengths get a specific diagnostic, metadata validation remains before data-device
work, and the old final assert is replaced by explicit metadata refusal. Primary
error text survives cleanup. Constant buffer capacity and existing optional-fstat/
local/qid behavior remain; no remote device transaction is promised. Callbacks
must not throw or publish a partially constructed display to concurrent users.

## Verification

The new fixture links actual init/close/refresh-discard/parser/channel/Rectangle
bodies, real guarded native allocations and independent FD/channel/lock/Image
ledgers. Native I/O, allocator/retirement and lock mechanics are explicit services.
160 cases/49,775 active predicates pass under local O3 nonrecovering UBSan.
Every reachable normal/fallback allocation/I-O refusal is exercised, including
FD0, partial channel captures, each standard image, optional stat, bad/short
records, maximum paths, full native qid, separate path owners and two live displays.
Eight exact-stage omission controls pass; crashes/compiler/resource/sanitizer/
timeout failures cannot count. Actual classic mk rebuilds libdraw.a successfully.

Focused native ARM64/x64 O3 ASan+UBSan jobs and the complete native64 pipeline
include the new positive/control fixture. Source-bound hosted results are pending
publication. No physical RV64, OS device, refresh-worker/collector/renderer,
managed CALL or Android/AWM acceptance is inferred.

## Direct coordination and remaining scope

Read/preserve FLASHVM-0124 atbe12f77. Its local reproduction of Inferno0122's exact
typed checkpoint and cubic fixtures agrees with our source-bound receipts.
Native atomic root replacement and its independent lifetime boundaries remain
peer-local; no native root/concurrency implementation is imported across heaps.
The agreed7 bounded passes/12 refusals/4 unsupported/2 malformed typed outcomes,
private-policy distinctions and owning-execution closure are unchanged.

Bounded Draw consumer names remain89/89; that is not resource/GC/GUI readiness.
Managed registration remains closed. JIT fast ARM64/x64166/173, RV64M103/173,
C dispatch173/173, default numeric43/173, ordinary WORD32 LEA and immutable named
launch/cache selection remain. No wire/feature/ABI ID or Frame64 layout changes.
The next integrated gates are managed modules/imports, real root/GC/resource/
refresh lifetime and graphical startup on the target. No APK/date is claimed.
