# CVTAC ASCII reuse on x64 and ARM64

CVTAC now has a guarded native path on x64 and ARM64. It converts a byte array
of 1 through 256 ASCII bytes into an existing unique ASCII String with sufficient
capacity. All metadata, type, lifetime, address-span and byte checks complete
before the first write. Copying follows memmove order; the captured length and
owner are published afterward. This also restores an owner slot which overlaps
the destination payload. The source REG pointers, reference counts, collector
state, FP controls/status and callee-saved registers are preserved.

The native path requires the exact builtin Tarray/Tstring identities, a live
source owner, an ordinary root-H array, and a descriptor with size 1 and no
pointer map. Shared, Rune, short or generic old destinations, non-ASCII input,
empty/large arrays, rooted views and error cases retain C. Descriptor pointers
and both payload spans are checked for native address wrap. The independent
layout validator rejects overlapping Array fields and insufficient buffers
publish no partial code.

C now captures the source Heap/Array records and destination owner before an
overlapping copy or release can change them. It validates low/wrapped source
headers, borrowed descriptor fields and backing-owner lifetime. Only a live,
unique exact String with a valid ASCII capacity can be reused; generic pointer
temporaries are never read as String records. C's reuse also handles empty and
larger ASCII arrays. Non-ASCII input keeps the existing UTF decoder, including
truncated-tail behavior. Allocation failure retains the old owner and source.
Nil input checks the previous owner's lifetime before publishing nil, avoiding
reference underflow on a dead destination.

These checks validate geometry and required metadata; arbitrary nonwrapping
pointers are not certified as mapped. The builtin array identity is appended to
LeafLayout and supplied by the compiler adapter. Private x64/ARM64 staging grows
from 80 to 128 four-byte units; emitted CVTAC leaves use 99 units on x64 and 92
on ARM64 for the ordinary fixture layout. Existing caller capacities remain
honored atomically. Dis, container and public runtime ABIs do not change. RV64
and the excluded older JIT backends are unchanged.

Validation from the repository root:

    PYTHONOPTIMIZE=1 python3 libinterp/tests/byte-string-jit.py
    python3 libinterp/tests/byte-string-layouts.py
    python3 libinterp/tests/byte-string-native.py
    BYTE_STRING_NO_ERRORS=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/byte-string-native.py
    LEAF_RUNTIME_BACKEND=amd64 python3 libinterp/tests/leaf64-runtime.py
    LEAF_RUNTIME_BACKEND=arm64 python3 libinterp/tests/leaf64-runtime.py
    python3 libinterp/tests/slice-tokenize-suite.py --core
    python3 libinterp/tests/jit-coverage.py

- Instruction emulation: 20,920 ARM64 and 20,934 x64 cases across both WORD
  models. Checks cover metadata rejection, full-width type identities, signed
  lengths, exact bounds, overlap in both directions, whole images, same-value
  writes, FP state, callee-saved registers and every insufficient caller capacity.
  Checks remain active with optimized Python. BYTE_STRING_ARCHES/EMITTER_DIR
  select an alternate backend/header.
- Layout/publication: 1,474,410 cases per backend with ASan/UBSan, both WORD
  widths, 21 independent fields, offsets -8 through 2056 and all sampled
  capacities through 128. Private staging and caller canaries remain intact.
- Actual C/x64 RX: 77,568 reuse, shared/generic/Rune owners, Unicode, overlap,
  source/result slots inside the payload, failure and FP cases per WORD model;
  1,216 additional owner/descriptor error cases per model. Full runs use UBSan
  and NDEBUG, including injected allocation errors and exact error identities.
- Successful C/RX paths additionally pass ASan/UBSan: 28,544 cases per WORD
  model. BYTE_STRING_NO_ERRORS omits longjmp/error injection because ASan stack
  unwinding cannot obtain process stack maps in this environment. It does not
  disable the full UBSan/error run or any bounds/image checks in selected cases.
- Existing source-validation and UTF round-trip matrices keep their allocation
  failure coverage by using non-reusable/shared old owners. The old C source
  fails the new low-owner span test with pointer arithmetic overflow under
  UBSan. The owning fixtures supply the new builtin identity for real code
  publication, rather than bypassing the production adapter.

Active native availability is now 143/173 on x64 and ARM64; 30 commands remain
interpreter-only. These counts include guarded partial paths. RV64 stays at
103/70 for M, 90/83 for Zmmul and 87/86 for I. ARM64 instruction execution is
emulated; physical target acceptance remains separate.

The following CVTCA series centralizes the shared byte-conversion layout checks,
including disjoint descriptor reference fields, and adds the reverse native
export path. Current counts and validation are in
[README.string-bytes-native.md](README.string-bytes-native.md).
