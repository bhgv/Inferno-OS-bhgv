# Raw and pointer list payloads in the AMD64/ARM64 JITs

ICONSM and ICONSP now have native handlers on AMD64 and ARM64, raising active
opcode availability from 153 to 155 of 173 IDs; 18 remain interpreter-only.
RV64 availability is unchanged. Counts indicate a native path, not universal
operand coverage or complete VM/OS acceptance.

ICONSM validates the full WORD count and native source/destination spans before
calling the common cons allocator. Its previous destination owner is staged in
a local native-stack cell, so allocation never overwrites an aliased raw source.
After allocation the handler copies up to 256 bytes into fresh node storage and
only then publishes the result. Empty copies do not inspect source data but
still allocate a node. Larger valid copies retain the checked C implementation.

The C raw constructor now checks source and output spans before allocating its
unpublished node. Nil, sentinel and wrapping addresses cannot leave a partial
allocation or replace the destination. The preflight oracle rejects the parent
interpreter, which contacted the allocator before checking these invalid ranges.

ICONSP captures a full native pointer, validates live/nonsaturated source and
Tptr references, then calls cons for allocation and tail transfer. Source retains,
Setmark, Tptr retention, List.t and payload stores are emitted directly. Heap.t
remains Tlist; List.t becomes Tptr. All fallible checks precede owner changes.
The generated code reloads live collector and descriptor cells after allocation;
it never freezes mutator colors or reference values at compilation time.
Allocation errors unwind through the existing runtime boundary. Rejected native
preflight returns to the interpreter for the established error precedence.

Pointer allocation uses 24-byte SysV AMD64 or 16-byte AAPCS64 native staging;
raw copying saves its local owner, source, output and count in a 56-byte AMD64 or
48-byte ARM64 frame containing the saved LR. Stack alignment,
nonvolatile registers, LR, Dis PC and operand-pointer metadata are preserved.
The native scalar/copy/GC instructions add no FP effects; runtime callbacks keep
their existing C semantics. Pool allocation and tail ownership stay in the shared
runtime helper rather than a duplicate architecture-specific allocator.

An exact lower-bound defect is also repaired: an owner at sizeof(Heap) would
put its heap header at address zero. Checks now require a strictly greater
address before reading that prefix. This applies to cons/consp, NEW/NEWZ/MNEWZ,
NEWA/NEWAZ, CVTAC/CVTCA and converted-array roots, with matching AMD64/ARM64 guards.
Reduced-address source tests model the same strict boundary. These are arithmetic
and ownership-contract checks, not validation of arbitrary unmapped addresses.

Evidence across WORD32/native pointers and legacy WORD64:

- payload-cons-native.py: 135,936 CONSM and 9,728 CONSP C/actual AMD64 RX cases,
  plus 32 raw preflight cases. It verifies delayed publication, full-WORD limits,
  overlap, exact copies/padding/canaries, nil/dead/saturated owners, allocation
  failure, error unwind, floating-point state and fresh GC/Type cells changed
  inside allocation. Live owners and both descriptor counts balance afterward.
- payload-cons-jit.py: 128,434 AMD64/ARM64 instruction-emulation checks. The high
  runtime allocator identity clobbers caller-saved registers; oracles verify
  stack/LR restoration, full VM images, bounded writes, high source owners,
  duplicate head/tail ownership, saturated boundaries and live post-call GC state.
  Simulated allocator errors stop at helper entry; actual AMD64 VM unwind is
  checked by the native driver, not inferred from the ARM64 model.
- payload-cons-layouts.py: 3,931,760 ASan/UBSan layout/capacity/canary checks per
  backend, including overlapping list fields and GC/reference cells.
- Successful native subsets pass ASan/UBSan (21,504 cases); complete errors run
  under UBSan and NDEBUG. Explicit resource counts and canaries accompany this
  container's ASan process/stack-discovery warnings.
- Existing record, array, scalar-cons and byte/string C/RX drivers now include
  exact nil-prefix rejection. The byte-conversion oracle fails against the
  parent source with UBSan; the repaired implementation rejects before reading.
- The core interpreter/library/Limbo checkpoint passes 112 host drivers. Native
  publication and the private C/ctypes layout probe also pass.

Private sizing layouts currently use 48/56 AMD64 units and 45/68 ARM64 units
(raw/pointer), within the unchanged 128-unit budget. Physical ARM64 execution
and complete Inferno runtime acceptance remain separate.
