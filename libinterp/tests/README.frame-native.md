# Pointer-free FRAME leaves and channel Type reservation

AMD64 and ARM64 now implement FRAME when the selected Type has no pointer map
and the current stack extent has strictly more bytes remaining than Type.size,
matching the interpreter's extension threshold. Module links, owner modules,
type tables, signed table counts, full-WORD indices, Type pointers, frame sizes,
pointer-map counts and stack bounds are checked before any VM write.

Success advances REG.SP, writes Frame.t and a nil Frame.mr, and publishes the
frame pointer through the destination slot. Other frame bytes and all other REG
fields are preserved. Operands can reside in frame storage about to be written.
Pointer-bearing types and frames that meet or exceed the remaining extent use the interpreter
path, which retains initmem/extend ownership. Both emitters keep the existing
64-unit limit. The private LeafLayout now carries explicit stack/frame offsets;
production publication derives them from the real C declarations.

The C frame bounds check also rejects H as either stack endpoint before pointer
arithmetic. The existing nil, size and reversed-extent checks remain in force.

Channel construction had a separate mapped-Type retain after allocating a
channel and optional buffer. cnewc now preflights the complete retain count:
buffer element, mapped mover, and any aliased Tchannel or Tarray header Type.
It checks independent header descriptors before allocating queues or owners.
This prevents signed overflow or late partial construction when a descriptor
has capacity for only part of the requested retains. Unused descriptors on
ordinary unbuffered channels remain unused, preserving their existing contract.

Validation:

- frame-native.py compares production C and published RX AMD64 code in 3,072
  cases per WORD model: exact stack images, REG and slot aliases, strict extent
  boundaries, map/extent fallback and invalid stack endpoint errors. Real stack
  growth and mapped initialization retain their separate runtime tests.
  Replaying the previous C frame bounds helper fails the H-endpoint regression.
- frame-jit.py executes 12,288 independent image/write/FP checks per backend,
  covering WORD32/native-pointer64 and WORD64, null/H metadata, malformed sizes,
  negative/wide indices, map fallback, stack bounds and writes into aliased slots.
  It checks native sizing, decoding, malformed layouts and bounded publication.
- channel-retains.py checks 72 descriptor/alias/buffer/mover cases per WORD model,
  including the last legal retain and failure before allocation/publication.
  Parent-source replay fails. Removing either header-alias reservation also
  fails independently.
- Selected ASan/UBSan checks cover the actual RX frame path and channel tests.
- All 84 --core host drivers pass. Actual native publication, fallback table,
  metadata failure/retry and unmap checks also pass.

| Backend | Guarded native opcodes | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 135 | 38 |
| ARM64 | 135 | 38 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

There are 173 active Dis instructions excluding INOP and unused IECLR. Counts
are guarded partial paths, not complete operand coverage or a complete native
VM ABI. ARM64 is checked by instruction emulation; physical execution remains
outstanding. No MIPS, SPARC, PowerPC, Tk, security or crypto source is modified.
