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
