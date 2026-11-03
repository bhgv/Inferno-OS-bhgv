# Native64 series0101: conservative raw-byte scalar local authority

Series0100 whole-image runtime tests still supplied trusted local declarations.
The new private dis64callinfercheck entry takes NONE. It opens one immutable
complete container, parses the existing canonical export/signature/frame catalog,
seeds parameters from raw signatures and derives exact numeric spans from decoded
instruction roles. Sorted insertions reject overlap, misalignment, header/result
cells, inconsistent kinds and more than128 slots per function. Derived workspace
is disposable and is never an execution capability or published proof.

The policy intentionally admits only Int32, Big and Real kinds. Bool signatures
and ambiguous W reinterpretation are refused, not inferred from width. Numeric
moves, the existing bounded W/L arithmetic/shift/comparison roles, Real moves/
comparisons and int32-to-real conversion contribute facts. Unsupported operations
refuse. This is a conservative subset of the existing private call verifier,
not new language restrictions or a new serialized schema. A local's fixed kind
cannot change even on unreachable paths. Direct accesses alone establish a kind,
NOT initialization: the existing complete definite-assignment/return/setup proof
must still succeed before final output publication. Special FRAME/CALL/LEA,
prepared-frame stores and outgoing result-tail reads remain separate roles.

The signature parameter seed is required even for unused incoming arguments.
Big shifts require an Int32 count separately from the Big value. No native pointer
cell is declared as a number. Exact widths/kinds, canonical call target/frame,
successful-return-only results, FRAME invalidation, CFG and leaf returns remain
checked by the existing semantic verifier. No arbitrary manifest can authorize
local kinds. There is no allocation or runtime/cstep/cache permission here.

The previous trusted API remains for diagnostics. Structural image opening was
factored out without widening its five-section/no-data/no-import/scalar-map policy;
all original25 identities and5 semantic controls still pass. The new diagnostic
poisons all externally supplied local declarations and supplies only raw bytes
to inference, then uses the existing private actual Module/heap/frame/interpreter/
JIT constructor. Nine accepted images and30 atomic refusals yield312 actual C/RX
runs across extension, saved registers, quanta and callee/allocation errors.
Four strict controls restore dependence on external declarations, omit signature
parameter seeds, publish before final proof or confuse W kinds. Each requires
semantic10 before unsafe continuation; signals/timeouts/compiler/sanitizer failures
are not acceptance. The unused-parameter positive catches omitted parameter seeds.

The14 additional immutable typed/error .hex files and SHA256 manifest are in
utils/tests/goldens/dis64-infer. That manifest has NO local declarations. Decode
bytes.fromhex; hashes identify bytes and do not authenticate input. Original25
peer goldens are unchanged. Run python3 utils/tests/dis64-call-infer.py; explicit
--write-goldens is only for a reviewed corpus revision. Default checks exact bytes.

Local x64 O3 nonrecovering UBSan passes. Native ARM64/x64 ASan jobs and full
classic-mk now include this gate; their outcomes are separate target evidence.
Previous0100 full41639394800 completed successfully on native ARM64/x64 and
RV64/QEMU. New-source acceptance must be recorded separately.

Production file admission remains43/173; FRAME/CALL/RET/LEA are not opened.
This private source-derived authority is one prerequisite, not a production
readmod/parsemod integration or independent FlashVM executable-call agreement.
Imports/managed values, Bool authority, loader/runtime capabilities and full
AWM/resource/device integration remain separate. No new opcode/section/feature,
wire layout, native Heap/Type/Frame or ordinary WORD32 LEA change. No concurrent
mutator/GC work is part of this series. JIT fast-path populations remain unchanged.
