# Native64 series0120: populated font-cache growth and refusal ordering

2026-11-04, experiment. This series repairs actual populated native font-cache
execution needed below managed Draw. It does not change JIT opcodes, the shared
Dis64 wire format, managed module admission or Android sources.

## Corrected failures

`loadchar` grew a Cachesubf table by DSUBF entries but copied the NEW population
from the OLD allocation. It now copies only existing records and initializes the
new suffix. Young retained sources survive growth; allocation refusal keeps the
existing eviction fallback. No VM Heap/collector assumption is introduced.

Before resizing an image, loadchar previously updated Font.width/maxdepth. Image
allocation/initialization refusal could leave the old image paired with new
geometry. Desired values are now local until fontresize successfully commits.
Both resize and glyph-loading paths now stop on preliminary flush refusal.

Glyph value/width/position/bearing previously changed before a failed preliminary
flush or packet reservation. They now publish only after those fallible local
steps. This is not a remote rendering acknowledgment: Draw retains its buffered
transport contract, and final server rollback is not claimed. Subfont lookup may
still warm/retain its source cache on a later glyph refusal.

Font.width remains short and Cacheinfo.x remains ushort on native64 hosts. Resize
now rejects negative/unrepresentable widths, out-of-policy populations and an
unrepresentable final cache-cell origin BEFORE allocation/I/O. Existing record
layouts and packets are unchanged. Successful cache-table replacement still
requires the established caller retry; allocation-refused replacement retains
its old table after committing the usable image.

## Verification

The new fixture compiles actual loadchar/fontresize/cachechars, Draw declarations,
UTF decoder and geometry helpers. It uses real exact-size table allocations and
independent copy-capacity observations; native image/subfont/transport services
are explicit adapters.24 scenarios/979 active predicates cover growth1/5/49,
eviction53, source retention, initialized suffix, allocation/transport refusals,
post-resize failures, replacement retries, cold/warm batches and short/ushort
capacity limits. Ascent adjustment, renderer and collector are not exercised.

Local native/signed/unsigned-char O3 nonrecovering UBSan runs pass. Five strict
semantic omission controls pass, stopping before unsafe continuation. Existing
subfont lifecycle passes147 active checks with debug assertions enabled. Its
source-extraction boundary now follows the loadchar signature instead of comment
wording. Actual classic mk compiles font.o and updates libdraw.a successfully.
Focused native ARM64/x64 O3 ASan+UBSan jobs and the full native64 pipeline include
all modes/controls; exact-source hosted results are pending source publication.
No physical RV64, full-matrix or Android/AWM acceptance is inferred.

## Dis64 coordination and remaining work

Read and preserve incoming FLASHVM-0121 at8f84122: peerff5a7bb reports native
string branches and independently replayed Inferno0119's bounded geometry fixture.
This is peer-reported native evidence, not imported concurrency or shared Frame64
execution authority. No multithreading task is opened in Inferno.

Requested next independent checkpoint: typed roles/function ownership and
initialization/result/LEA semantics on the same immutable25-image corpus, with
per-image verdicts/stages for23 canonical and2 malformed outer images. Structural
binding alone does not grant call admission. No wire/feature/ABI ID, Frame64
layout, default43/173, ordinary WORD32 LEA or immutable named dis64-calls launch/
cache change is made. Owning execution remains outside that requested parser proof.

Bounded Draw consumer count stays84/89 and managed registration remains closed.
ARM64/x64 fast166/173, RV64M103/173 and C dispatch173/173 are unchanged. Console
ARM64 has prior execution evidence; full managed imports/calls, roots/GC/resources
and integrated Android/AWM startup/rendering remain. No APK/date is claimed.

Concurrent FLASHVM-0122 at2972714 was read before source publication. Its REAL
branch protocol explicitly separates coherent operand capture from later true-
target observation and releases all native claims before FP evaluation/unwind.
This remains a peer-native contract, not a shared Frame64 change or an Inferno
import. The incoming-only commit is preserved; the typed-role request above stands.

## Exact-source native target receipt

Source82d5275be9404068d716232b9f8102ea1511fc17, workflow41786196960:
ARM64 job126428774969 and x64 job126428775125 both completed successfully.
Actual logs read:979 active predicates in each native/signed/unsigned-char mode
and all five exact-stage controls under O3 ASan+UBSan, ordinary leak detection
enabled. This supersedes pending status for these focused cache jobs only.
No full-matrix, physical RV64, renderer/collector/managed CALL or Android/AWM
acceptance is inferred. The same production source remains unchanged.
