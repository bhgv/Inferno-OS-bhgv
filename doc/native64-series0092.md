# Native64 series0092: native font roots and graphical descriptor publication

## Production defects and fixes

Freetype used legacy reference-slot constants for native Face/Glyph descriptors.
On LP64 WORD32/ref64, familyname/stylename are at24/32 (native slots3/4), so
Face needs0x18. Legacy0x06 instead selects the private FT handles at40/48.
Glyph.bitmap is at24 (slot3,0x10); legacy0x02 selects slot6 beyond its32-byte
native record, and dtype refuses it. These are native C maps, not shared wire
type declarations.

Freetype now derives maps from actual C fields with nativefieldmap; Matrix and
Vector maps stay empty. Private native FT handles stay unmarked. Prefab keeps
its existing managed-field selection, but stages maps locally. Both initializers
check all dtype/registration results, release staged creator owners on failure,
preserve diagnostics through cleanup, reject partial states unchanged and make
complete initialization idempotent. Caller holds VM initialization exclusion.
No Tk/security/crypto or native font-rendering implementation changes.

## Actual-source checks

New graphics initializer fixture compiles real PElement/PCompound/Face/public
record declarations and actual dtype/freetype/native maps/protected-error bodies.
Registration and graphical/FT destructor identities are injected boundaries.
Per WORD32/64, Prefab14 and Freetype23 failure/retry/partial cases verify exact
map bytes, every callback identity, publication and balanced creator ownership.
Three staging controls per module plus two restored-legacy-font-map controls
require semantic status10. Compiler/signal/sanitizer failures do not count.
Fatal controls clean tracked fixture memory after the failed predicate so
intentional leaks cannot mask it with LSan; positive balance is checked unaided.

The existing real heap map suite now includes four Freetype records alongside
Draw/Prefab. Actual heap/initmem/incmem/freeptrs test1072 allocation/copy/alias/
release combinations per model, checking public references and every excluded
byte. Its registration observation now returns a nonnil identity as required
by the checked constructors; it still does not register modules. Generated
predicates remain active under NDEBUG. Its existing LSan opt-out is retained;
tracked ownership and ASan bounds are distinct from LSan certification.

Local x64 O3 nonrecovering UBSan passes both initializer models, all eight
strict controls and1072 heap/map cases per model. Full Prefab/Freetype consumers
pass syntax checks with fresh classic host-Limbo tables. Prefab is commented
out of default libinterp OFILES; explicit consumer checks do not activate it.

Classic-mk ARM64/x64/RV64 and focused native address jobs gain these gates.
New target results are pending publication. Prior0091 full matrix41587971674
completed successfully on all three targets; its focused/Bionic results are
separate evidence and do not certify this change.

## Coordination and remaining work

FLASHVM0083 heap-count conversion report is received as hosted adapter evidence,
not imported or locally replayed. Native ulong Heap and signed-int Type counts,
no post-drop object reads, high-gchalt retirement, source acquisition, roots/marks
and complete C/JIT exclusion remain separate protocol obligations. No new shared
Dis64 ID, native pointer serialization or file CALL/LEA8 activation is added.

This fixes data descriptors and failure handling. It does not admit the complete
Freetype/Prefab export ABI, test native FT rendering/transform scalar ABI, start
AWM or replace physical-device acceptance. Native paths remain ARM64/x64
166/173, RV64I/Zmmul/M87/90/103; complete native+C173/173. Production file gate
stays43/173.


## Android fixture correction

Initial Android run41594788023 compiled the core/full consumers and passed the
new Prefab/Freetype initializer/maps and eight controls, then failed the legacy
heap-map fixture at compilation. Android lib9.h defines object-like allocator
aliases before interp.h; the old fixture subsequently replaced free with a
function-like tracker, leaving Type.free_ inconsistent with dtype's free field.
No APK success is claimed from that run.

The shared test-only portfixture helper now accepts an explicit include boundary
(default unchanged). GUI maps remove aliases immediately after lib9.h, before
record declarations and libc transports. Host synthetic-alias O3 UBSan passes
both1072-case models and both initializer modules. The native graphics address
step injects those aliases too, so Linux catches this class before another
Bionic-only compile failure. Production map/initialization source is unchanged
from05591a39; fresh source-specific address/Bionic checks are required.


## Source-specific native address evidence

Corrected source143fd9c218ea02ea106a64cc5b9b4a1fe4dc8347 passes focused native
ARM64/x64 ASan/UBSan run41596068733. Both uploaded ZIP digests were checked
against provider metadata, then their graphics logs read: Prefab14/Freetype23
cases per WORD32/64, all eight strict semantic controls, and1072 real heap/map
cases per WORD model on each architecture. This run also injects the allocator
aliases neutralized by the corrected fixture. The older05591a39 address run
41594038173 passed separately; the corrected result is the applicable proof.

ARM64 address artifact13212741655 SHA256:
e5aeec83dad805b65902b3aa8a0f9dc1832ca5fcc626251e2158948234de0d18.
x64 address artifact13212436784 SHA256:
72627ae0d18cfcfb01f540aaf7eda9ce50dd929fb3bd8a4c4da015b32e54b5f7.

Corrected full classic-mk matrix41596069454 and Android41596693062 are running.
The earlier05591a39 full matrix was superseded/cancelled, not passed. Android
launcher8d853fb5e16c636012756bee71f5c33b09f6c95e pins corrected core143fd9c.
No APK or full GUI acceptance is inferred from the focused address result.

## FLASHVM0085 acknowledgement

Received incoming journal through823935e9 and read MODULE-COUNTS.md at
e9bf28ca2024d1939c2e25529baea297cea78df9. Checked signed-int Module counting,
LAST-only registry/code cleanup, no post-release count reads, separate Type
ownership and distinct clone MPs agree with owning audit boundaries. Source
lookup/registry protection and code retirement still require exclusion; atomic
preflight is not an allocation reservation. This is documentary review, not an
import/replay of the hosted adapter or a new file/module ABI admission.

Their independent05591a39 initializer replay confirms14/23 cases and eight
controls; it does not certify our corrected Bionic build or1072-case maps.
No shared format/layout/opcode IDs or parallel managed admission change.
