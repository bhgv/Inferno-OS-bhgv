# Ownership preflight checkpoint

MOVP, SELF, HEADP and TAIL now check decoded pointer-slot spans before reading
operands. SELF ignores its unused source slot. HEADP and TAIL check arithmetic
for the complete selected List pointer field before loading it or redirecting REG.s.
The AMD64 and ARM64 emitters also reject source Heap-header underflow before
reading reference counts or colors. These are arithmetic checks; they cannot
establish whether an arbitrary address belongs to a mapped allocation.

Source lifetime errors precede reads of the previous destination owner. The C
handlers capture and validate the previous owner before retaining, marking or
publishing the source. This prevents partial ownership changes when the previous
owner is nil, too low to have a Heap header, or already dead, including when the
destination aliases source reference-count metadata.

A declined JIT path preserves REG, heap bytes, slots and propagation state.
The C handlers preserve the existing HEADP/TAIL convention: after a valid List
field is selected, REG.s stays redirected if MOVP subsequently reports an owner
error. Slot and List-span errors occur before that redirection.

Validation on both WORD32 and WORD64 models:

* 5,424 native C preflight/error-atomicity cases.
* 771,712 native C differential ownership cases.
* The native fixture also passed with NDEBUG, ASan and UBSan. The host sanitizer
  emitted its documented no-return stack warning around the setjmp fixture.
* 1,600 AMD64/ARM64 emulator preflight cases, checking allowed reads, mutation
  atomicity, source-error priority, unused SELF source slots and FP state.
* 193,896 additional emulator memory/GC/width/alignment/FP cases.
* All 128 selected host core drivers passed across resumed checkpoints.

C predicates remain active under NDEBUG, and Python predicates under -O.
Coverage remains 164 of 173 active Dis commands on AMD64 and ARM64. This series
repairs existing guarded paths; it adds no newly native opcode. Physical ARM64
execution and a complete Inferno boot were not checked by these fixtures.

The separate `owner-optimized.py` checkpoint runs all 1,600 preflight cases
under Python `-O` on both architectures. It clears interactive family filters
and forces both architectures, preserving compiler/dependency settings. This
keeps assertion-independent checks exercised in the registered JIT suite; the
full `owner-jit.py` differential matrix remains registered separately.
