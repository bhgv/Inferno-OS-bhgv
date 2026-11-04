# Native64 series0119: checked direct-subfont placement and width

2026-11-04. Branch experiment, following source f6f47ff and accepted native
ARM64/x64 default-font receipts. This series changes a separate direct subfont
path in libdraw, not JIT opcode coverage or the Dis64 wire format.

## Production changes

`stringsubfont` previously formed left/right/top/bottom and the next pen in
signed Int32 arithmetic without range checks. A valid bearing or width near an
Int32 endpoint could overflow before the packet layer saw a rectangle.
`strsubfontwidth` similarly accumulated unlimited glyph widths in signed int.

Both paths now validate adjacent/vertical metrics before use. Native vlong
arithmetic checks every placement coordinate and the next pen before narrowing.
An unrepresentable glyph is refused before Draw, preserving the last complete
pen. Width measurement returns the measured prefix before overflow. Existing
out-of-range-rune skipping and real UTF decoding remain; nil inputs are handled.
The void Draw API and packet layout are unchanged. Device acceptance cannot be
inferred from the returned pen. Font/image/metrics lifetime remains the caller's
responsibility; the helper does not prove pointer provenance or array capacity.

## Verification

The new actual-source fixture uses production Draw declarations, UTF decoder,
Point/Rectangle constructors and both subfont consumers. Only the drawing
boundary is observed. A wide independent oracle covers all256 bearings, four
widths and64 Int32 coordinate pairs, plus Unicode, absent/truncated input,
malformed metrics, prefix refusal and real text exceeding Int32 total width.
There are213,420 active predicates in each native/signed/unsigned-char run.

All three local O3 nonrecovering UBSan runs pass. Three exact-stage semantic
controls pass (metrics, placement, width); no compiler/sanitizer/signal/timeout
is accepted as control evidence. Actual classic mk rebuilt stringsubfont.o and
updated libdraw.a successfully. Hosted ARM64/x64 ASan+UBSan jobs and the full
native64 target pipeline include this fixture; exact-source hosted results are
pending at source publication. There is no new physical RV64 or Android proof.

Bounded Draw C-consumer count remains84/89; managed registration remains closed.
ARM64/x64 native fast paths166/173, RV64M103/173 and C dispatch173/173 are unchanged.
These ratios do not express whole-port, GUI or Dis64 execution readiness.

## Direct FlashVM coordination

Read and preserve FLASHVM-0120 on3a1bb6f: peer4f97aabee reports guarded native
String length, native tests and a replay of Inferno0118's source-bound bootstrap
fixture. Treat that as peer-reported native evidence, not Inferno collector or
shared Frame64 admission. No native concurrency implementation is imported here.

The immutable25-image shared corpus,23 structural bindings and2 malformed outer
refusals remain separate from pending typed-role/function-ownership,
initialization/result/LEA semantics. No wire/feature/ABI ID, ordinary WORD32 LEA,
default43/173 or immutable named dis64-calls launch/cache agreement changes.
Full managed imports/calls, roots/GC/resources and Android/AWM startup/rendering
still require integrated acceptance. No APK or completion date is claimed.
