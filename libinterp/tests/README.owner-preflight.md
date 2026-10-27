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


## Aligned heap identities with packed operand slots

MOVP, SELF, HEADP and TAIL now reject source Heap identities lacking native
pointer alignment before reading reference counts or collector metadata. C also
rejects an unaligned previous destination owner before retaining or publishing
the source. Operand cells themselves may remain unaligned: their pointer values
are copied with `memmove`, as before. H still represents an empty reference.

AMD64 and ARM64 emitters perform the same source alignment check before any
Heap field load, retain, mark or destination publication. A distinct previous
owner already declines to C without dereferencing that owner. Invalid leaves
preserve REG and all heap, slot and collector bytes. This is a lifetime guard,
not an arbitrary-pointer mapping validator.

The native fixture now links the production `cjitexec`, supplies real optab
entries for these four operations, and compares direct C, guarded fast execution
and complete published JIT entries, including actual C fallback. It selects
AMD64 or ARM64 from the host; the run for this series was on AMD64. WORD32/64
both retain native eight-byte pointers.

Validation:

```sh
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/owner-native.py
../jit-venv/bin/python libinterp/tests/owner-jit.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

The second command needs Python with Unicorn and Capstone installed; substitute
that interpreter's path on another checkout. Each scalar model passed 3,160
preflight cases (both direct C and complete native fallback) and 385,856 owner
comparison scenarios across the three routes. Guest execution passed 96,948
layout/ownership checks plus 856 preflight checks per architecture, including
read hooks that reject any Heap metadata access for misaligned source owners.
The preceding C bodies fail with UBSan on source/previous-owner misalignment;
the preceding AMD64 and ARM64 emitters independently fail the forbidden-read
oracle. These negative controls use the current driver and unchanged preceding
production bodies, rather than a deliberately broken implementation.

Opcode counts remain 173/173 complete entries including C and 166/173 guarded
native paths per priority architecture. This guard does not complete the native
compiler/reference/frame ABI or replace physical ARM64 validation. RV64 and
MIPS/SPARC/PowerPC JITs, Tk and security/crypto are unchanged.
