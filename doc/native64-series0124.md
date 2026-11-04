# Native64 series0124: public Screen channel ownership

2026-11-04, experiment. Following0123's native display-init rollback, inspection
found a definite native constructor mismatch: publicscreen returned image=nil,
while _allocwindow unconditionally read s->image->chan. Creating a window on a
public screen could therefore dereference nil before image allocation.

Screen now retains its validated native channel independently of its optional
borrowed image/fill references. Both official factories initialize it; window
creation uses it and refuses invalid screen/display/channel/ID handles before
image allocation or list effects. Public construction refuses invalid input
before native allocation or transport. Owned construction also refuses closing
connections/invalid backing channels. Success-only list publication, reused-header
identity, refresh attachment, existing packet shapes and retry policy remain.
The native Screen header changes: rebuild all native consumers coherently.
Managed structures, Frame64 and shared Dis64 ABI/feature IDs are unchanged.

The actual four native constructors, public headers and channel/geometry codecs
pass local O3 nonrecovering UBSan: 188 cases/7,245 predicates, nine channels,
owned/public references, reuse, wire bytes, independent allocation/reservation/
flush/image refusals, guard bytes, malformed handles and high native channel bits.
Six exact-stage omission controls pass. The image allocator/I-O are bounded
services, not actual renderer/GC/refresh-worker evidence. Classic mk rebuilt
libdraw.a successfully. Native ARM64/x64 ASan+UBSan jobs are wired; their exact
source receipt must be recorded after publication. No APK/device acceptance.

Read/preserve incoming FLASHVM-0126 at8f3f587. The peer explicitly corrected its
previous REAL C-tail completion claim: the old oracle accepted declining fallback;
new completion/execute-once evidence is peer-reported atd9bef4ac. We do not import
its guarded concurrency machinery or treat the corrected claim as Inferno tests.
Dis64's immutable25 verdicts remain7 bounded passes/12 refusals/4 unsupported/2
malformed, and owning execution remains closed. No wire changes requested.

Readiness remains: bounded Draw consumer names89/89; JIT native fast ARM64/x64
166/173, RV64M103/173; C dispatch173/173; default Dis64 numeric admission43/173.
These are separate coverage measures, not whole-port readiness. Linux ARM64
console has prior execution evidence. Managed modules/imports/DATA, actual GC/
root/resource/refresh lifetime and target graphical startup still gate Android/AWM.
