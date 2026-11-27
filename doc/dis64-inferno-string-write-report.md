# INFERNO-0190 — local String writes, concatenation and slicing

2026-11-15. Base 5b8c1e15 /0189. The selected importing-client and entryless
Command.init library paths now admit ADDC, INSC and SLICEC through the existing
C handlers and guarded JIT. No String/Heap layout, public opcode/section/kind,
private signature, runtime handler or JIT emitter is changed.

## Compiler and admission

Typed lowering derives String operands from actual declarations/layouts.
Concatenation reads two String roots and produces an exact mapped FP String
root. An implicit middle operand in `s += other` is made explicit from the
checked destination; no address is guessed. MP String operands use the existing
literal relocation, including the ADDC middle operand.

Character writes consume initialized int value/index operands. Slices consume
initialized int start/end operands. Both read and replace a mapped FP String
owner. Numeric MP, pointer/numeric role confusion, unmapped/misaligned targets,
Context/List targets and oversized middle immediates refuse before construction.
The definite-initialization CFG records the read and write separately. Literal
MP cells stay readonly; shared owners are copied by the existing runtime.

The earlier pool limits (32 literals,255 Runes each,1024 aggregate) describe
serialized readonly constants, not the maximum length of a computed String.
General mutable DATA/array/ADT payloads, extra imports/exports, channel/Draw/Tk
resource startup and arbitrary prepared-call expressions remain outside this
selected profile. Some fused inline expressions still need lowering; explicit
int locals for character results are exercised here.

## Reproduced evidence

- 93 Command pairs (70 previous +23 new), each96 roots on x64 and ARM64/Linux
 QEMU: C/RX, scheduler quanta1/3/1000, reload, admission/open refusal and
 dontcompile, with actual gc.c epochs between quanta. New cases cover ASCII,
 Unicode/supplementary/NUL, nil concat, append/self-append, shared literals,
 retained aliases/lists, widening, character append, slices including empty/nil,
 in-place slice and negative/gap/reversed/end bounds errors. Importers also
 concatenate, write and slice before explicit MCALL.
- 12 scalar-library pairs,1152 roots per target, test both scalar result binders
 with newly computed Strings. Prior62 scalar pairs/5952 roots also pass with
 actual GC: combined167 pairs/16032 roots on each target.
- 54 independently rewritten raw images on each target cover all three opcodes
 in provider/importer, valid controls, MP destinations, Context destinations,
 misalignment, frame extent, middle/source role confusion, uninitialized numeric
 input and middle immediate width. Removing root-class matching admits the
 forged INSC Context target; the pure proof control reports semantic exit10.
- Previous literal36, Command41, Context38 and reference41 raw groups and controls
 pass again. Host ASan/UBSan covers constructors for all93 pairs and provider
 allocation failures; empty/full-bit Rune constructor witnesses pass on both
 targets. Whole-VM ASan and concurrent collection are not claimed.
- Rebuilt complete consoles pass348 main C/RX Command roots per target, including
 valid/missing providers, plus hash/default-policy/entryless-library controls.
 The updated identical RU/EN tutorial executes concat, copy-on-write and slices.
 Prior214 scalar main console roots pass again.
- 186 deterministic Command files and62 previous scalar clients compile. All140
 previous Command images and62 scalar images retain exact bytes. All260 compared
 classic outputs (including12 focused scalar clients) match their baseline.
 Eight unsupported source boundaries still refuse with no emitted payload;
 obsolete concat/mutation negatives now check array DATA. AWM body preflight
 remains14 accepted/346 refused with unchanged classic code.

Drivers: `dis64-command.py`, `dis64-client-literal-scalar.py`,
`dis64-client-string-write-proof.py`, `dis64-command-boot.py`,
`dis64-command-producer.py`; gates integrated into `native64-port.py`.
[English example](dis64-command-init.md), [Russian example](dis64-command-init.ru.md).

## FlashVM review

At the user's request, reviewed bhgv/flashVM master
[0604bbfc](https://github.com/bhgv/flashVM/commit/0604bbfc8f7feaccb1f27b47a5e793cbc230477c),
including `disVM/upstream/libinterp/string.c` and the String store/slice native
reports. That entire C String implementation is byte-identical to this Inferno
checkout. Its existing helpers are reused, not reimplemented or imported.
The reports describe native x64/ARM64 unique-owner INSC/SLICEC paths and shared/
allocation C fallback. Those peer test counts were read, not replayed here.
No native no-exclusive DisVM runtime, debugger or GC migration is performed;
existing vendor pin7f224145 remains unchanged. Current Inferno peer journal
still ends FLASHVM-0202/0203. This receipt is recorded in the own journal.

Android/Bionic/device GUI, a usable AWM APK and physical ARM64 execution remain
unverified by this Linux/QEMU series.
