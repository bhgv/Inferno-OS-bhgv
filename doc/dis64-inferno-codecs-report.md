# INFERNO-0191 — numeric/String and UTF-8 byte-array conversions

2026-11-15. Base7b08ae5a /0190. Selected importing clients and entryless
Command.init libraries now compile the existing CVTCW/CVTCL/CVTCF,
CVTWC/CVTLC/CVTFC and CVTCA/CVTAC instructions. Byte-array length, nil tests,
identity and mapped pointer copies are admitted too. Existing C handlers and
JIT paths are reused; no runtime handler, emitter, wire section/kind, private
signature or native layout changes.

## What changed

Typed lowering derives numeric widths and managed operand classes from actual
Type/Decl storage. Strings and byte arrays have distinct internal proof classes.
Every produced managed value occupies an exact mapped FP root. String sources
may use readonly MP literals; numeric MP, prepared-frame destinations, indirect
operands and middle operands remain refused. Word immediates are allowed only
for int-to-String; big/real require initialized typed FP cells. Equal-width big
and real storage remains distinct. Codec classification leaves unrelated operand
roles untouched on refusal.

The frame map now includes array locals, but no array initializer or additional
serialized element descriptor is introduced. CVTCA creates the normal runtime
Tarray/Tbyte owner; CVTAC accepts only a root proved to have that origin, a nil
root or a compatible alias. The selected temporary allocator keeps array
Type identity instead of borrowing a String/Context temporary declaration.
This enables `s = string array of byte s` without changing classic allocation
of temporaries. Prior images remain byte-identical.

String/byte-array conversion uses UTF-8, preserving embedded NUL and supplementary
characters by explicit length. `len string` counts characters; `len array` counts
bytes. Aliases keep their original owners when a destination is replaced or
reused. String-to-number uses existing VM parsing semantics; this is not a new
strict parser or a new guarantee about out-of-range input. Formatting is the
existing VM operation, including its normal real-number spelling.

General array creation/indexing/slicing, nested payloads, mutable DATA, extra
imports/exports, managed return signatures, arbitrary ADT/channel operations,
real Draw/Tk resources and full AWM remain outside this selected file profile.

## Evidence and practical limits

- 125 Command pairs (93 previous +32 new) execute96 scheduler roots each on x64
 and ARM64/Linux QEMU:12000 roots per target. The new cases cover signed int
 limits, big values above32 bits, negative big, decimal/exponent real parsing,
 numeric formatting, Unicode/NUL, empty/nil, aliases, reuse, loops, array identity
 and fused String/array conversion. Importing clients convert values before
 explicit MCALL. Actual gc.c epochs run between quanta1/3/1000 in C/RX, reload,
 failure/admission and dontcompile modes.
- 20 focused scalar-library pairs exercise both scalar result binders with these
 converted values:1920 actual-GC roots per target. Prior62 scalar pairs/5952
 roots pass again: combined207 pairs/19872 roots on each target.
- 130 independent raw images cover eight codecs plus LENA in library/importer,
 root-class substitution, big/real equal-width confusion, numeric definitions,
 misalignment, FP extent/modes, MP writes and illegal middle/immediate roles.
 Host ASan/UBSan and ARM64 pass. Removing managed root-class matching admits
 a forged Context-to-String conversion; the proof control exits10 semantically.
- A separate GC omission control preserves other mapped roots but skips actual
 byte arrays in the callee frame. The independent graph observer detects the
 lost owner before guest resume, semantic exit10 on both targets. The observer
 now follows array backing roots and checks byte element geometry. Production
 GC policy is unchanged. All String caches and byte-array/element Type references
 return to fixture baselines after retirement.
- Rebuilt complete consoles pass476 main C/RX roots each (119 all-source pairs,
 valid/missing provider), plus hash/default-policy/entryless controls. Actual
 lib9 parsing/formatting and the identical updated RU/EN tutorial execute here.
 The smaller extracted-handler fixture uses a libc formatting boundary for
 the two standard formats; it does not substitute for complete-console evidence.
- 250 deterministic Command files,186 prior Command files and62 scalar files are
 checked against the0190 compiler. All332 compared classic outputs, including
 the20 focused scalar clients, match. Baseline tools built in a separate tree
 receive an explicit identical ROOT so source-path metadata is compared fairly;
 code or metadata is not stripped or rewritten. Eight unsupported source cases
 still refuse without payload.
- Previous literal36, String-write54, Command41, Context38 and reference41 raw
 groups and their controls pass. Host ASan/UBSan constructor-only checks cover
 all125 pairs and provider construction failures. This does not claim full-VM
 ASan, conversion-time allocation-failure enumeration or concurrent collection.
- General AWM preflight remains14 accepted/346 whole-body refusals, unchanged
 classic output. Android/Bionic GUI/APK, physical ARM64 and a usable AWM client
 are not established by these Linux/QEMU gates.

Drivers: `dis64-client-codecs-proof.py`, `dis64-client-literal-scalar.py`,
`dis64-command.py`, `dis64-command-boot.py`, `dis64-command-producer.py`.
The raw codec and byte-array marking controls are integrated in native64-port.py.
[English tutorial](dis64-command-init.md), [Russian tutorial](dis64-command-init.ru.md).

## FlashVM coordination

Reviewed current FlashVM master
[2fe0645b](https://github.com/bhgv/flashVM/commit/2fe0645bebaf2f0228b26d0cb7855554132ae5a2).
Its `disVM/upstream/libinterp/string.c` remains byte-identical to this Inferno
checkout. Existing codecs are connected to compiler/admission here, not copied
or reimplemented. No DisVM migration or peer concurrency/debugger code import;
vendor pin7f224145 stays unchanged. Current incoming journal remains
FLASHVM-0202/0203; the receipt is appended only to the Inferno journal.
