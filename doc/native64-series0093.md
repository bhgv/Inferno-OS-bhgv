# Native64 series 0093: Freetype scalar and raster boundaries

## Production changes

The Limbo wrapper previously cast Matrix/Vector WORD records directly to the
fixed-int bridge. That fails for WORD64. The native bridge then cast those int
records to FreeType FT_Matrix/FT_Vector, whose fields are native long, failing
on LP64 even with WORD32. Both boundaries now copy named fields into local
records. Checked conversion refuses WORD64 scalar narrowing before native work.
Nil transform semantics and fixed-point units are preserved.

Hosted FreeType configuration now uses target limits instead of hardcoding
ULONG_MAX to 32 bits. Legacy non-hosted toolchains keep their existing fallback.
Two stream macros used left shifts on negative signed bytes. Multiplication
now decodes the same signed high-byte values within the destination range.
The real vendored renderer reached this UBSan failure while reading Vera.ttf;
it is corrected, not suppressed by disabling shift instrumentation.

Glyph copying validates negative dimensions, checked width*height, signed pitch
magnitude, row extents and a nonnil buffer before allocation/copy. Negative
pitch remains supported; row stepping avoids int multiplication and advancing
past the last row. Empty rasters perform no pointer arithmetic or memcpy.
The native renderer bridge refuses non-gray nonempty raster formats and native
long advances that cannot fit its public int fields before publishing a view.

Character-query results are published only after reacquiring VM exclusion.
Face creation checks path duplication failure and handles absent native family/
style names as empty managed strings. Its managed owner remains published
before fallible conversions; failed native construction retires that owner.

## Validation and boundaries

Actual entry bodies use real native VM/FreeType/frame declarations. Only heap,
error and renderer transport boundaries are injected in the isolated oracle.
WORD32/WORD64 execute 452/453 raster/transform/refusal cases: padded and signed
strides, empty rasters, invalid dimensions/pitch/buffer, both glyph allocation
failures, face/path/native failures, nullable names and scalar/result ordering.
The native bridge checks 1024 signed stream patterns in both byte orders, cursor
advance, target width macros, all optional transform combinations and unchanged
output on glyph refusal.

Five strict controls restore old VM/native transform casts, short-pitch admission,
hardcoded long limits or premature result publication. Each must reach its
semantic status10; compiler/signal/sanitizer errors never count. Native transform
controls have adjacent bounded records so the old cast can fail its predicate
without an out-of-bounds read. Fatal controls retire fixture allocations; no
LSan opt-out is added. Predicates use if, including under NDEBUG.

A separate executable compiles the actual vendored engine, production bridge
and lib9 sorter, using the retained libc file adapter instead of Inferno's
namespace adapter. Vera.ttf and NotoSans-Regular.ttf render through identity,
translation and 90-degree rotation. Translation bounds and rotated advances
are checked independently. Local x64 O3 nonrecovering UBSan passes this real
rendering smoke and both isolated bridges; all five controls also pass with
synthetic Android allocator aliases. Existing full consumer syntax and
initializer/map gates remain separate. Fresh native/Bionic results are pending.

This does not register the complete native64 Freetype/Draw ABI or start AWM.
No Tk/security/crypto, MIPS/SPARC/PowerPC JIT, shared format ID or file CALL/LEA8
activation changes. Native fast paths remain ARM64/x64 166/173, RV64I/Zmmul/M
87/90/103; native+C dispatch remains 173/173, scalar file admission 43/173.
Those opcode counts do not measure overall port readiness or completion time.


## Native address result and Android compiler follow-up

Initial source efbf6042 passes native ARM64/x64 ASan/UBSan41601134425, including
the actual vendored renderer and all five initial controls. Android41601304711
stops while compiling the standalone engine: modern Clang rejects a const-void
comparison callback passed to Inferno's void-pointer qsort callback type. This
is a real retained-library ABI mismatch, not an APK success or semantic control.

All five sorter callback signatures now match the actual Inferno prototype,
including the TrueType forward declaration. Type1 kerning and PostScript Unicode
comparisons also use relational ordering instead of narrowing an unsigned
difference. The native oracle uses actual record declarations and checks72
ordered key pairs spanning the signed boundary and all-ones values. A sixth
strict control restores the old kerning subtraction. Engine compilation now
treats incompatible pointer types as errors on GCC too. New source-specific
native/Bionic checks follow; earlier results do not certify this correction.
