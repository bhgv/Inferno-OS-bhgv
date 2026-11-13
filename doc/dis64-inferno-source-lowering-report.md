# INFERNO-0167 — typed storage feeds native FP/MP argument execution

2026-11-13. Base b1806799 / INFERNO-0166. Continues the type-driven layout work
toward the existing Inferno Word32/ref64 Android client. FLASHVM-0180 remains
the last received peer entry; its original-owner routing/concurrent runtime
is not imported. No shared wire IDs or public managed ABI are assigned.

## Producer integration

The existing native Sys variadic argument producer now uses `dis64layoutstorage`
for complete FP/MP source banks and `dis64layoutaddress` for exact argument reads
and MP inputs of local definitions. The old descriptor-only root-widening
algorithm is removed. Word values stay four bytes; eight-byte scalars retain
alignment; roots widen independently. Padding, cross-field scalar reads and
borrowed native addresses cannot become ordinary source arguments.

Source records carry policy `typed-word4-ref8` and full typed leaves/maps.
An unsupported bank retires its partial plan before returning. A regression
fixture borrows the production constructor and proves empty state and balanced
allocation after a later unsupported declaration follows an allocated root.

Nested inline global initializers resolve through original Type/Decl/Node
structure, retaining Unicode/NUL byte lengths and signed64/real bits. A checked
direct MP-to-FP MOVMP can define projected fields subsequently read as arguments.
Each projection retains the original opcode/descriptor/source/destination in
`cell.aggregate`, with the original PC. The producer checks exact field identity,
width and the existing straight-line/branch/exception/call boundaries. Shared
copy PCs require the same actual copy; they are not mistaken for multiple
independent whole-copy executions. Source AST/IR and serialized bytes stay intact.

## Runtime and independent evidence

- AMD64 UBSan and ARM64 UBSan/QEMU pass the emu/os, WORD32/WORD32-file matrix.
  Each combination retains 768 real FRAME/MFRAME/MCALL/GC cases through four
  Sys gateways, plus the existing formatter, write, ownership and failure cases.
- Three new programs cover nested global fields, explicit local definitions and
  fields projected from `seed := original`. Real VM MOVW/MOVP/MOVL/MOVF/CVTWL
  handlers execute the recipes. Generated `.m` declarations and C offsetof/sizeof
  independently check the field origins (Outer grows 32 to 40 bytes).
- Owning mapped storage retains actual MP inputs, FP fields, gather cells and
  destination references through GC between steps. Exact Word sign extension,
  signed64/real bits, UTF/NUL runes and three/four-owner reference counts pass.
  This is trusted source-recipe replay, not executing a general new Dis64 caller.
- The `typed-field-origin` omission must fail a semantic predicate before reading
  a wrongly relocated field. Selected source-map/reference/current-MP omissions
  also pass both architectures and all four port/model combinations. The broader
  pre-copy host matrix passed all existing and field-origin controls; final-copy
  runs retain all positives and the explicitly selected controls.
- Independent classic Dis decoding rejects four corruptions of copy origin,
  descriptor identity, source base and field projection. Ordinary code remains
  byte-identical with diagnostics. Existing 4096-slot admission/4097 refusal,
  nested call provenance, branch/call barriers and unsupported-source refusals pass.
- The final compiler passes ASan/UBSan source generation and all 376 AWM storage
  plans. All sixteen ordinary AWM outputs match the prior compiler; managed
  refusals retain the same plans. 191 private managed/Sys image cases remain
  byte-identical. The identical RU/EN example compiles with four copy projections.

The runtime fixtures use UBSan. The separately documented runtime-ASan unwind
issue is still open; compiler ASan does not close it. QEMU does not provide
physical Android/Bionic/device, ordering or performance acceptance. These tests
use Inferno's VM exclusion and do not certify a concurrent FlashVM adapter.

## Remaining Android path

This connects generic typed storage to an executing private native-argument
consumer. It does not add general managed image emission/admission or ordinary
variadic frame-tail startup. Full copy lowering must preserve aliases, ordering
and owners of every field, including unread fields. General CFG, payload
descriptors, table writers, exceptions and complete call/result lowering remain.
The AWM startup closure still refuses Dis64 compilation; a working GUI APK is
not claimed. No guard was bypassed or new source profile silently admitted.

[English guide](dis64-typed-source-lowering.md) · [Русское руководство](dis64-typed-source-lowering.ru.md)
