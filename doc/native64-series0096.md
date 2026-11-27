# Native64 series0096: full Draw declarations and safe scalar consumers

The entire production libinterp/draw.c can now select the fresh actual Draw
int32/ref64 interface with DIS_NATIVE64_DRAW_HEADER. All legacy method/frame
spellings have explicit qualified aliases. Missing or zero DIS_WORD32 refuses
at compilation. Legacy declarations/tables remain the default. Header-only
mode refuses drawmodinit before allocation instead of registering a legacy
frame table against the new declarations. No Draw startup/device is activated.

Ten real production entries now build complete scalar/tuple/rectangle results
locally and copy with memmove. This supports unaligned caller destinations and
partial argument overlap without typed result dereferences: Font.bbox,
Chans.eq/depth, Display.rgb2cmap/cmap2rgb/cmap2rgba, setalpha, icossin/icossin2,
and bytesperline. Font.bbox remains its existing zero-rectangle placeholder;
this series does not claim an implemented font bounding-box calculation.

Four underlying arithmetic defects are corrected in their owning libraries:

* setalpha and cmap2rgba assemble high colour bits with unsigned shifts.
  Display.rgb uses unsigned shifts too; its allocation/device path is compiled
  but not executed by this scalar test.
* rgb2cmap uses wide component differences and unsigned wide distance sums;
  even INT_MIN/INT_MAX arguments select the mathematically nearest palette
  entry, retaining first-entry tie behavior and existing normal-range results.
* bytesperline/wordsperline multiply in vlong and use signed mathematical
  floor/ceil. Negative maximum coordinates previously rounded incorrectly,
  and large coordinates could overflow before division. Public int results
  retain the low32 bits; this is not a new allocation-size validation contract.
* icossin2 widens before absolute value and ratio scaling, including INT_MIN.
  The documented tables/interpolation and normal-coordinate behavior remain.

native64-draw-scalars.py compiles the complete actual draw.c plus real chan,
rgb, arith, bytesperline and integer-trigonometry sources. Link section
collection discards uninvoked device/resource code; it is not device, refresh,
font ownership or window execution evidence. Independent Python arbitrary
precision arithmetic and mathematical table endpoints supply the expected
results, not duplicate C result calls. Tests cover all256 palette indices,
extreme/random RGB inputs, alpha bits, negative/large rectangles, integer
angles, wide vector directions, zero placeholders and channel descriptors.
Every byte offset and partial source/result alias is checked with guard bytes.
Native wordsperline is also invoked against independent word-stride results.

Local x64 O3 nonrecovering UBSan:291096 active predicates per WORD32 and
WORD32-file; missing/zero model guards, actual header-only initializer refusal
and strict truncated-result semantic exit10 pass. Existing Draw initializer
WORD32/WORD64 ownership tests and three retirement/publication/registration
controls are separate regression gates. No production GC/managed callbacks
are emulated by the scalar arithmetic test. Native ARM64/x64 ASan and full
ARM64/x64/RV64 port gates include this test; read CI results separately.

Preceding0095 native ARM64 and x64 ASan/UBSan have both passed. Its full classic
mk matrix was still running when this series was prepared; the newer full
matrix repeats those gates. JIT opcode coverage and scalar file admission are
unchanged. The remaining full AWM blockers include complete Draw registration,
resource/device/refresh lifecycle, managed calls/imports and scheduler/GC proof.
No shared Dis64 IDs/layout or native pointers on wire are changed.

## Published acceptance

Production source: d580379a38627f744b2867fb76b2fbf9824f4b3e.
Native ARM64 and x64 ASan/UBSan both pass41619620436, including this
new scalar consumer, geometry and registration/ownership gates. Actual logs
confirm291096 predicates per WORD32/WORD32-file, early model guards and
strict scalar result control on each native architecture. The same
new scalar test also passes local synthetic port aliases under O3 UBSan.
Complete classic-mk41619620285 is still running and is separate evidence.

The owning89-export inventory has19 geometry bodies in geom.c and70 in draw.c,
with no missing definitions. Executed coherent-frame coverage is29/89 functions
(19 geometry plus10 scalar); the remaining60 have compilation evidence only.
This is function-level C ABI coverage, not JIT opcode coverage, state-space
coverage, complete Draw registration or full AWM readiness.
