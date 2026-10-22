# IA-32 JIT publication, Type helpers and operand audit

This series extends the integer arithmetic work on experiment. It preserves
existing VM layout and does not add an AMD64/ARM64 backend or a Dis64 ABI.

## Confirmed defects and fixes

- Type helper sizing used a fixed 4096-byte temporary buffer. Dense maps can
  emit far more than that. The temporary now has a checked 128-byte bound per
  map byte, plus a prologue/return block. A second sizing pass determines the
  final allocation. Negative/overflowing map lengths are rejected. Type fields
  are published only after both emitters and segflush succeed.
- The compile driver allocated only size patch entries, although a handler
  range may end at nprog. It now reserves size+1 and records the end offset.
- tinit was uninitialized, and zero-Type modules depended on malloc(0).
  Required Type flags are now zeroed, with zero-length allocations skipped.
- Module code was published, old instructions freed and export/link/handler
  PCs changed before segflush. Its range also used n*sizeof(base), despite n
  being a byte count, and errors were ignored. Code and its literal region
  are now synchronized with their exact allocated byte range before publish.
- Case/goto tables were marked and relocated in the original data during
  fallible generation. Compilation now uses a shadow Module and private data
  copy. Export/link PCs, handler metadata, entry and original tables remain
  unchanged until all generation, cache and Type helper work succeeds.
- Failure or VM error unwinds reclaim unpublished code, temporary arrays and
  data, and restore only Type helpers created by that attempt. Existing Type
  helpers may be shared and are preserved. Successful comvec preambles remain
  reusable; unpublished preambles are reclaimed and are never exposed before
  cache synchronization. The runtime kprotect boundary owns the unwind.
- Native movl stored its low half before reading the high half. A destination
  four bytes after the source corrupted the copied value. Both halves are now
  captured before either store. movf uses the same bit-preserving integer path.
  A host x87 probe reproduced 0x7ff0000000000001 becoming 0x7ff8000000000001
  under the old fldl/fstpl sequence. The move no longer touches x87 state.
- ARM word comparison selected CMN by evaluating -source_immediate, invoking
  signed overflow for INT32_MIN. Magnitude selection now uses unsigned negate.

## Common native-PC preflight

jit-validate.h is used by both IA-32 and ARM32 and is listed in mkfile header
dependencies. Before emission it validates branch PC alignment/range, immediate
frame Type indices, static module-data case/goto offsets, headers, counts,
complete table extents, target PCs and default targets. Exact table aliases
from compiler folding are accepted; partial or different-kind overlaps are
rejected because one table's relocation can corrupt another's count/targets.

Invalid metadata returns compilation failure without changing the module.
This is a preflight for compile-time indexing, not a verifier for arbitrary
Dis operand memory accesses during execution. Table-overlap checks scan earlier
table instructions and can add compilation cost to table-heavy modules; no
compile-time or runtime benchmarks have been performed.

## Reproducible checks and boundaries

```
python3 libinterp/tests/x86-publish.py
CFLAGS='-O1 -g -fsanitize=undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/arm-publish.py
python3 libinterp/tests/x86-types.py
python3 libinterp/tests/x86-integer-jit.py
python3 libinterp/tests/arm-branches.py
```

Unicorn/Capstone are required for native encoder tests (the full test suite
also uses Keystone).

- x86-publish executes production driver, runtime protection, preamble, Type
  publication and table/exception relocators. It injects VM errors at 52 event
  points and retries, exercises allocation/cache failures, old/shared Type
  preservation, phase mismatch, literal sizing and already-compiled modules.
  Its cache boundary checks the exact module code/literal extent. Integer
  encoding, Type instructions and scheduler calls are stubs in this harness.
- Both x86/ARM publication tests cover malformed metadata, exact table aliases,
  rejected overlaps, 32,768 single-byte data mutations and 129 data extents.
  The mutation loop checks preflight bounds/purity and allocator balance; it
  does not execute mutated bytecode. ARM additionally retains its 50 VM-error
  injection points.
- x86-types executes production initializers/destructors for empty, sparse and
  dense maps up to 4096 map bytes. Its 135 checks include memory canaries,
  allocation/cache failures and generated code above the old buffer limit.
  Destructor CALL targets remain intact and address a permanent recording
  return stub that clobbers volatile registers; the real reclaim thunk is
  exercised separately by jit-lifetime.py. Full native GC remains out of scope.
- x86-integer-jit now performs 55,176 arithmetic, routing, move and conversion
  checks, including overlap, source/destination indirection, NaN payloads and
  unchanged surrounding memory. Helper calls are recording stubs.
- arm-branches executes 11,232 signed word comparison cases, including source
  and middle immediates and INT32_MIN, with UBSan. Operand loads, scheduler and
  final Dis target relocation are boundary stubs. A MOVT encoding bug in the
  older VFP test's constant-load stub was corrected as part of this coverage.

All 33 libinterp Python suites pass. Full xec/IA-32/ARM source syntax checks
pass with their respective Linux headers. These host checks do not boot an
emulator/OS, prove executable-memory permissions, validate physical cache
coherence or substitute for target builds and module-unload testing on hardware.

Permanent reclaim thunks, full-address ARM calls and helper-PC publication
are covered in [jit-lifetime-audit.md](jit-lifetime-audit.md).

IEEE IA-32 branch semantics and overlapping block moves are covered in
[jit-float-move-audit.md](jit-float-move-audit.md).

Byte immediates, integer branches and ARM indexing are covered in
[jit-index-audit.md](jit-index-audit.md).

Native stack allocation, nil module calls and signed link selectors are covered
in [jit-frame-audit.md](jit-frame-audit.md).

Checked module link resolution, native guards and export cleanup are covered
in [module-link-audit.md](module-link-audit.md).
