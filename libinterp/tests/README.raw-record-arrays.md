# Raw record reuse and checked VM array construction

For the subsequent NEWA/NEWAZ native paths, see
[README.array-constructor-native.md](README.array-constructor-native.md).

INEW now has guarded native paths on AMD64 and ARM64. It joins INEWZ and IMNEWZ
in one record-construction emitter, with named modes for local clearing,
imported clearing and raw construction. Each backend has native paths for
147 of the 173 active Dis opcode IDs; 26 remain interpreter-only. RV64 counts
are unchanged. These are conditional paths, not complete coverage of every
operand or allocation case.

Raw NEW leaves scalar bytes unspecified. Reusing a unique standard pointer-free
record therefore requires no payload loads or stores. The emitted code still
checks live Module.type lookup, the full-WORD index, descriptor/header spans,
positive type references, exact freeheap/markheap identities, matching owner Type,
a single owner reference, size 0 through 256, and the complete nonempty payload
span. The C interpreter applies the same rules without the native size limit.
It does not reset colors, profiling metadata, references or GC state. A result
slot is published after all guards. Aliased indexes are captured beforehand.
Shared/mapped/custom-finalizer objects use ordinary heap allocation; NEWZ/MNEWZ
retain zero initialization. Raw custom-resource fields must be initialized by
the application before their custom finalizer can use them, as before.

NEWA and NEWAZ previously bypassed heaparray's pointer-map validation and called
nheap/initarray directly. A negative map length, excessive map, or bitmap bit
outside a complete element could reach pointer initialization after storage had
already been allocated. Both commands now share a checked helper which routes
through heaparray, retaining both descriptors only after validation. This also
removes duplicated header construction.

The helper validates the full WORD count with acheck before converting to
heaparray's int parameter. Values above INT_MAX must never wrap into small or
zero arrays. NEWAZ clears scalar storage and restores H into pointer cells
without a second element-Type retain. Zero-count and zero-width arrays skip
payload clearing. Unaligned VM operand cells are read/written with memmove;
invalid descriptor alignment is rejected before heaparray dereferences it.

Evidence, for both WORD32/native-pointer and legacy WORD64 configurations:

- newzero-native.py: 253,440 C/actual AMD64 RX comparisons across NEW, NEWZ and
  MNEWZ, plus 102,960 heap/heapz map cases and 30 live-module-span rejections.
  Checks cover aliases, custom native finalizers, mapped/shared owners, stale
  cached tables, failures, full images, references, profiling and FP state.
- newzero-jit.py: 154,656 AMD64/ARM64 instruction-emulation, ABI, high-address,
  callback-identity, owner-write-span and atomic publication checks.
- newzero-layouts.py: 5,265,750 ASan/UBSan layout/capacity/canary checks per
  backend, covering all three record modes.
- array-create.py: 208,896 NEWA/NEWAZ map/stride/scalar-zero/alias/failure/canary
  cases. It includes full-WORD counts above 32 bits and allocation-size bounds.
  Another 576 cases check saturated/dead descriptor references, shared Tarray
  descriptors requiring two retains, and error precedence. The same map oracle
  rejects the parent xec.c, which accepted invalid maps.
- Successful allocation/cleanup subsets also pass ASan/UBSan. Complete error
  matrices run under UBSan with NDEBUG; resource and trailing canary checks are
  explicit because this container emits ASan process/stack-discovery warnings.

These drivers are wired into the core/JIT checkpoint suite. ARM64 execution is
emulated; physical target and complete Inferno runtime acceptance are separate.
