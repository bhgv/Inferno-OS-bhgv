# INFERNO-0192 — zeroed byte arrays and owning slices

2026-11-15. Base d60a9414 /0191. Selected clients and entryless Command.init
libraries now construct byte arrays and take slices through existing NEWAZ and
SLICEA handlers/JIT. Existing byte-array conversions, length, copies and identity
continue to work. No runtime handler/emitter, public kind, section, signature or
layout changes.

## Constructor, descriptors and views

The trusted compiler checks the actual byte Type/Decl descriptor before lowering
array allocation. Both ordinary NEWA input and explicit NEWAZ input become NEWAZ
in this selected profile, providing zeroed scalar bytes. Classic emission keeps
its existing choice. Raw NEWA is deliberately refused, avoiding unspecified byte
contents in admitted images. Counts must be initialized int cells or immediates;
negative counts raise the existing negative-array-size error.

Only files actually using NEWAZ get an extra checked1-byte/alignment1/zero-map
Type descriptor after their existing descriptors: index2 without literals or3
with literals. Context retains its independent index1/2 and MP its existing map.
The new descriptor must be used, and every NEWAZ must select exactly it.
Malformed element size/map/alignment, frame/MP descriptor substitution and an
unused extra descriptor refuse before construction. Old images keep their exact
2/3-descriptor layout. The constructor translates this proved zero bitmap to a
native primitive Type with np0, so byte codecs recognize its pointer-free element
geometry; arbitrary maps are never dropped.

SLICEA consumes initialized int start/end and a mapped ByteArray owner. Aliases
are checked by the existing role/CFG proof. The runtime keeps ordinary array view
semantics: shared roots are retained, nested slices keep the backing owner and a
unique view may narrow in place. Empty and nil slices follow existing bounds and
nil behavior. Slice bounds count bytes, including UTF-8 and embedded NUL.
No new serialized array DATA initializer or general element type is admitted.

## Evidence

- 149 Command pairs (125 previous +24 new),96 scheduler roots each on x64 and
 ARM64/Linux QEMU:14304 roots per target. Counts/empty/negative, replacement,
 reuse, Context allocation with the byte descriptor present, loops, aliases,
 nested/shared/in-place slices, dropped source roots, empty/nil, UTF-8/NUL and
 bad bounds execute with actual gc.c between quanta1/3/1000, C/RX, reload,
 failure/admission and dontcompile modes. Importers allocate and slice too.
- 28 focused scalar-library pairs/2688 roots exercise both scalar result binders
 with the new values; prior62 scalar pairs/5952 roots pass again. Combined239
 pairs/22944 roots per target.
- 95 independently rewritten raw images cover new/old descriptor shapes, byte
 geometry, missing/unused descriptor, allocator substitutions, uninitialized
 NEWA, still-unported INDB, count/root confusion and slice operands/modes/spans.
 Host ASan/UBSan and ARM64 pass. Removing descriptor identity, used-descriptor
 or root-class checks yields semantic proof mismatch exit10, without execution.
- Backing-root marking omission preserves the array view but loses its backing
 owner. The independent observer follows Array.root and detects retirement
 before guest resume, semantic exit10 on both architectures. Production GC and
 refcount policy are unchanged. Heap, module and element-Type ownership returns
 to the fixture baseline. Constructor-only ASan/UBSan covers149 pairs and
 provider allocation refusals; no whole-VM sanitizer/concurrent-GC claim.
- Rebuilt complete consoles pass572 main C/RX roots per target (143 all-source
 pairs, valid/missing provider), plus policy/hash/entryless controls and the
 identical updated RU/EN tutorial. A dropped source buffer still leaves a live
 two-byte view in that example. Actual existing VM/lib9 paths execute here.
- 298 deterministic Command files; all250 previous Command images and62 scalar
 images remain byte-identical to0191. All388 compared classic outputs, including
 the28 focused clients, match with explicit identical ROOT. Eight unsupported
 source boundaries still refuse with no payload. Prior literal36, String-write54,
 codec130, Command41, Context38 and reference41 raw groups/controls pass.
- General AWM preflight stays14 accepted/346 refused, unchanged classic output.
 These Linux/QEMU tests do not establish Android/Bionic/device GUI, physical
 ARM64 or a usable AWM APK.

Drivers: `dis64-client-arrays-proof.py`, `dis64-command.py`,
`dis64-command-boot.py`, `dis64-client-literal-scalar.py`,
`dis64-command-producer.py`. Raw and backing-mark controls are integrated in
native64-port.py. [EN tutorial](dis64-command-init.md),
[RU tutorial](dis64-command-init.ru.md).

## Indexing boundary and FlashVM receipt

Reviewed FlashVM master
[c9dc3b4c](https://github.com/bhgv/flashVM/commit/c9dc3b4c075fa4aa697957094e0a74adc4317935).
Its newarray, slicea and arrayindex C bodies exactly match ours. No peer runtime
import or DisVM migration; vendor pin7f224145 is unchanged. Incoming journal
still ends FLASHVM-0202/0203; receipt is appended only to the own journal.

Direct byte element indexing is still refused. The shared arrayindex currently
casts the native element pointer to WORD and stores sizeof(WORD). Under
Word32/ref64 that cannot carry a general native address. Fixing it requires a
coherent borrowed-address lowering/slot and C/JIT agreement, not merely admitting
INDB. Native-WORD64 behavior is a separate model. This is a source finding, not
an indexing fix or a claim that indexed byte arrays execute here. Requested peer
review in the own journal. General arrays, indexed stores, payload initializers,
managed calls and real Draw/Tk startup remain further port work.
