# NEW/NEWZ/MNEWZ decoded cells and allocation publication

AMD64 and ARM64 now preflight the decoded module-pointer cell (MNEWZ), WORD
index cell and native-pointer result cell before dereferencing them. Null, H
and wrapping spans decline without changing VM memory. These checks validate
arithmetic extents, not whether arbitrary non-null addresses are mapped.
AMD64 checks the index-cell span before loading the module type count because
the span helper uses RDX as scratch. Both emitters remain inside 128 units.

The real C constructors apply the corresponding exBounds checks. MNEWZ keeps
its module error before reading the index. newrecord captures the destination
once, before reuse/zeroing/allocation, and publishes through that captured cell.
The heap allocation monitor can borrow R.d; publication must still target the
original decoded result. Callback register side effects are preserved.

Allocation, mapped initialization, ownership release and exceptional paths
remain in C. Native reuse still requires a unique pointer-free object with
matching Type and ordinary callbacks, with a payload of at most 256 bytes.
Raw NEW preserves the existing payload; NEWZ/MNEWZ clear it, then restore a
result cell that overlaps that payload. No opcode-count increase is claimed.

## Validation

- record-slots-native.py: 169,632 successful C, native-plus-C and complete-JIT
  routes, plus 228 rejected C/complete-JIT routes, across WORD32 and WORD64
  with native 64-bit pointers. Real heap/heapz/freeheap/initmem bodies exercise
  unique reuse, shared ownership, H, pointer maps, payload aliases, unaligned
  cells and allocation callbacks which redirect R.d. Every rejection also
  checks the native decline. UBSan covers the complete matrix; ASan+UBSan
  repeats the successful routes without disabling stack instrumentation.
- newzero-jit.py under Python -O: 83,178 AMD64 and 83,100 ARM64 checks,
  including malformed source/result/imported-index cells, independent byte
  images, write spans, preserved native registers and FP state.
- newzero-layouts.py: 5,265,750 sizing/publication/canary cases.
- Existing newzero-native.py: 253,440 constructor route cases, 102,960
  heap/map cases, 30 module lookup cases and six nil-prefix cases.
- Existing list-slot-jit.py: 6,094 cases per architecture, preserving incoming
  commit 152ab5a. Complete-dispatch native/ARM64 compiler and all-opcode
  coverage checks pass for both WORD layouts. Production xec.c compiles for
  both layouts with implicit function declarations treated as errors.
- Parent replay against 152ab5a rejects the previous implementation for the
  allocation callback and each decoded-cell family in C and both emitters.

Run the new C/RX matrix with `python libinterp/tests/record-slots-native.py`.
For the ASan repeat, set `RECORD_SLOT_NO_ERRORS=1` and
`CFLAGS="-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all"`.
Explicit error/longjmp paths are covered by UBSan separately because this
isolated fixture does not reproduce the complete VM stack-unwind environment.
`RECORD_SLOT_SOURCE`, `RECORD_SLOT_SECTION`, `RECORD_SLOT_MODE` and
`RECORD_SLOT_BAD` select C parent replay. `EMITTER_DIR`, `ZERO_ARCHES` and
`ZERO_PROFILES` select machine-code parent replay. Missing emulator packages
or a non-AMD64 native host fail explicitly.

The opcode table remains 173/173 complete JIT+C entries and 166/173 guarded
native fast paths (95.95%) for each priority architecture. Physical ARM64
execution, a complete 64-bit Inferno boot, multi-opcode compilation and
performance measurements remain outside this fixture evidence. RV64 is
still deferred; excluded backends and Tk/security/crypto are unchanged.
