# Series0118: read-only default-font bootstrap and native signed bearings

This series fixes bootstrap resource handling below Display construction, including
alignment of the compiled default font, exact source bounds, signed glyph bearings
and synthetic Font ownership. Shared Dis64 IDs/layout and managed Draw admission
are unchanged; there is no new JIT opcode or Android/AWM activation.

## Read-only built-in bytes, bounded metadata before I/O

getdefont previously tried to align defontdata by moving bytes inside the shared
array. On an unaligned location that changed/truncated the source, could write
past its exposed end, and left later loads parsing already shifted data. Its
header conversion used unbounded atoi and trusted returned upload counts before
locating metrics. The actual upload and packed-metric decoder use byte operations;
neither requires rewriting the source to achieve word alignment.

The loader now treats the existing5,958-byte compiled asset as read-only. It
checks each fixed-width decimal field, legacy depth-table index, pixel extent,
subfont count/metric representation, exact packed extent and monotonic/height-
bounded metrics before allocating or sending resources. Pointer advances stay
within the declared compiled source capacity. It requires a complete byte upload;
allocation, upload or subfont-construction refusal releases local image/metrics
owners while preserving the original diagnostic. The optional weak-cache name
allocation retains its previous non-fatal behavior. Local rollback is not a
promise to undo data already accepted by a remote server.

The classic-mk dependency list now includes pixelspan.h so an incremental rebuild
also recompiles its native readers/uploaders and this new user of the header.
No external file format or bootstrap asset bytes are changed.

## Explicit signed horizontal bearings

Fontchar.left was plain char, whose signedness differs across native compilers.
A negative horizontal bearing could therefore be interpreted as a large positive
one. It is now schar; _unpackinfo decodes the signed byte through a representable
integer value rather than a host-dependent narrowing. The native Fontchar layout
remains eight bytes on these targets; the packed record stays six bytes and its
byte encoding is unchanged. The separate Subfont.ascent representation/policy is
not changed by this fix.

## Synthetic range and ownership boundary

mkfont now requires a nonempty valid Subfont and rejects closing displays or
Unicode range overflow before allocating. A range ending at U+10FFFF remains valid;
min+n-1 cannot wrap into an unrelated range. Its prior ownership contract is made
explicit: success adopts one caller-owned Subfont reference; refusal consumes
none. No extra retain is added. Existing separate caller aliases survive Font
retirement when they own their own references. This is not an RC uniqueness,
managed root or concurrency proof.

## Actual-source bootstrap/ownership checks

The new fixture compiles actual getdefont, loadimage, allocsubfont, weak-cache code,
freesubfont, mkfont, freefont, channel/rectangle/extent helpers and _unpackinfo.
The immutable compiled asset is used as the independent byte identity, while
known pixel/metric dimensions and signed-bearing vectors check decoded results.
Native image allocation/release and packet transport remain bounded services;
actual allocated C objects are tracked with exact owners and guard extents.
No synthetic VM Heap headers or root authority are introduced.

Local O3 nonrecovering UBSan with platform aliases passes10,070 active conditions
for native, forced-signed-char and forced-unsigned-char models. Cases cover eight
source alignments, all constructor allocations including optional cache refusal,
repeated loads for simultaneously live Displays/default aliases, twelve malformed
metadata variants before I/O, each upload packet/final-flush refusal, diagnostic
preservation, six synthetic Font allocation failures, terminal/wrapped Unicode
ranges and separately held input references. Every native allocation retires.

Seven controlled omissions mutate source bytes, skip extent/upload/rollback/range/
reference/signed-bearing properties. Each reaches its exact named predicate;
compiler/resource/sanitizer failures, signals and timeouts are rejected. The error
stop safely releases fixture-ledger blocks without invoking invalid production
callbacks. Hosted ASan/UBSan retains ordinary leak detection. The loader does not
claim provenance for arbitrary memory paired with a dishonest capacity; its input
is the compiled asset and caller-owned native lifetime.

Existing font-input/corpus regression remains229,454 active conditions. The
existing subfont read/write/cache lifecycle regression passes147 checks with
conditions active. The actual libdraw archive rebuilds via classic mk after the
public header and private dependency change. New fast native ARM64/x64 ASan+UBSan
jobs and the full target pipeline run all three char models and seven controls.
Exact-source hosted acceptance is pending at publication.

No renderer/device, guest managed CALL, collector traversal, populated font-cache
lifetime or Android runtime execution is inferred. Bounded Draw C-consumer count
stays84/89 with managed registration closed. These are native bootstrap/resource
proofs, not completion of Display.allocate or the whole managed runtime.

## Prior receipts and shared-format coordination

Prior d17a763 font-input source has exact native ARM64/x64 ASan/UBSan acceptance:
126292715889 and126292715868 in41744117792, with229,454 conditions/all four omission
controls, as recorded in the f30a9c7 receipt. Those results do not accept this new
built-in loader or signed-bearing source.

Re-fetched experiment at f30a9c75d49b0f1efeb26d37793ba820f6836d8f; no newer incoming
reply after FLASHVM-0118 was present. The next independent typed-role/initialization/
result/LEA checkpoint remains agreed/pending on the immutable corpus before owning
execution. Native WORD64 source/field guarding is not the shared Int32/Ref64 contract.
Existing25 identities/23 structural passes/two malformed outer do not grant private21
semantic or executable authority. Default43/173, ordinary WORD32 LEA, immutable named
-X dis64-calls -d launch/cache discrimination and shared feature/ABI IDs stay unchanged.

Linux ARM64 console build/execution already has target evidence. Managed imports,
roots/collector/resource integration and Android/AWM startup/graphics remain open.
Fast native counts remain ARM64/x64 166/173(95.95%), RV64M103/173(59.54%), C support
173/173 dispatch entries. No whole-port completion percentage or unsupported APK
date is inferred from these counts.
