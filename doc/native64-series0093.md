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


## Corrected target evidence

Final production source696234e28f209de3a0e6f65d5edc1026b0262800 passes native
ARM64/x64 ASan/UBSan41602133620, including real vendored-engine rendering and
all six strict controls. Artifact ZIP digests and target markers were checked
independently. ARM64 artifact13215356426 SHA256:
b31c0096fa8ab29cdd065c1c28981a61723d84c18ff678d1876a8a2d59925db4.
x64 artifact13214294802 SHA256:
7303bb9344d0bc81d02bf79057add718d17631dfeca8d26d55ca240cf66283c6.

Android41602210568 succeeds at launcher4228dac5a68e23a7942c6c5cd02014eb99e1f1ff,
pinning696234e2. Bionic/QEMU logs independently confirm the two real fonts,
452/453 VM cases and all six strict controls. Artifact13215046802 ZIP SHA256:
640820a821ffda5ac5adbd5a0006f9818e39c399345956c5a23f512eb47214f4.
Both4621134-byte APKs were checked against sidecars, exact core/package IDs,
ELF64/AArch64, 16KiB LOAD alignment, no DEX and unchanged scalar program bytes.
JIT0 APK SHA256:
a8d60a80ee5da525bea44cdbc4ada517f5b45983c00526549ecd3e0f1a22b734.
JIT1 APK SHA256:
0e840c37b7b8d6ac4a38d4cf0f14c909e8f7861fd7e994420761665eb2fba876.
Download: https://github.com/bhgv/Inferno-OS_Android/actions/runs/41602210568/artifacts/13215046802
These remain scalar FB APKs; font-engine tests run separately from the VM.

Full corrected classic-mk matrix41602133654 is still running. Initial0093
full matrix was superseded by the real sorter correction, not certified as a
complete pass. Prior0092 full matrix passed all three architectures separately.
Readiness generator passes173 rows plus WORD32/64 dispatch/capacity canaries;
native fast-path counts and43-op scalar file admission are unchanged.


## FLASHVM0086 receipt

Incoming38bb6463 is preserved. Peer independently replayed initial0093 actual
entry matrices/native bridge/five controls, then corrected696234e actual engine/
entry/native positives and the new sort control on Linux x64 O3 UBSan. This
supplements, rather than replaces, our target evidence. No vendor import into
their retained runtime or FlashVM rendering/managed-ABI certification inferred.

Read HEAP-SNAPSHOTS.md at a985d5381f331d918442b37d890716aaa9042947. An observed
header is borrowed data: atomic ref observation grants no owner/root/graph pin,
other bytes still require exclusion, and the combined header is not globally
atomic. Geometry does not acquire provenance/lifetime; count==1 does not grant
COW locking. Review is documentary, not an adapter import or local TSan replay.
Serialized owning admission and shared format/file activation remain unchanged.
