# Array operand/header spans, captured C publication and chunked SLICELA

This checkpoint covers NEWA, NEWAZ, SLICEA and SLICELA on AMD64 and ARM64.

NEWA/NEWAZ validate decoded index, length and result cells before loading them.
C captures the original result address once and uses it after reuse/zeroing or
heaparray. An allocation monitor borrowing R.d can no longer redirect publication.
Raw scalar bytes, pointer-map initialization, alias restoration and descriptor
retains keep their existing behavior. AMD64 consolidates overlapping null/span
checks to keep the complete guarded constructor inside the existing 128-unit
budget; module counts are loaded after span helpers which borrow RDX.

SLICEA validates decoded cells, array/type header spans and backing reference
origins before metadata reads. C captures the result slot before allocating a
new view and releases the already captured old view, rather than reloading its
owner from a possibly borrowed R.d. Unique view/root reuse remains native;
allocation and mapped ownership stay in C. Native metadata owners require
8-byte alignment; decoded operand cells still support arbitrary byte alignment.

SLICELA checks its source cell first. A null/H source copies nothing and reads
neither the index nor destination cells, consistently in C and both JITs.
For a present source, used cells and complete selected array/type metadata spans
are checked before reads. Zero-length copies still skip the Type/data checks.
C arrayspan validates its Type address before reading size/map fields, and the
mapped fallback checks the source Heap origin before temporarily retaining it.
Arithmetic checks do not prove arbitrary non-null addresses identify mapped
storage; C knows the full Heap origin while private native checks use declared
reference-cell offsets.

## Native copy expansion

The bounded byte-array SLICELA path now accepts up to 256 elements instead of
128. It requires a byte-sized pointer-free destination Type and checked full
source/destination geometry. Both backends use their existing shared memmove
kernel: complete 8-byte blocks in the required overlap direction followed by
an exact byte tail. All guards run before copying. AMD64 retains the no-store
case for identical physical ranges and uses a checked near displacement to
skip the larger kernel. Ordinary flags may change; DF, FP state, REG, stack
and preserved native registers keep their contracts. This is an instruction
path improvement; no wall-clock speedup or full-VM performance is claimed.

The copy leaf and view fixtures now use the actual 128-unit publication budget.
Capacity/canary checks remain active. The copy C fixture uses explicit if/abort
checks rather than assert; the Python copy oracle also stays active under -O.

## Validation

- array-spans-native.py: 327,072 constructor and 61,440 allocating-view routes,
  564 slot-error routes, 144 header-error routes and 108 nil-source routes,
  across WORD32/64 with native 64-bit pointers. Real heap/heaparray/freearray
  bodies cover unique/shared/H owners, pointer maps, payload aliases, unaligned
  cells and callbacks borrowing R.s/R.m/R.d. Every operation is checked through
  C, native+C and real complete-JIT entries. UBSan covers the complete matrix;
  ASan+UBSan repeats successful and nil-source routes with stack checks enabled.
- array-spans-jit.py under -O: 3,348 AMD64 and 3,356 ARM64 cell/header/Type/root,
  nil-source, register/SP/LR/FP and publication-capacity checks.
- slice-copy-native.py: 5,431,392 independent geometry/overlap/type/offset/alias/
  mapped-fallback/FP cases per WORD layout. Successful ASan+UBSan repeats report
  5,431,344 cases per layout; explicit throwing cases are covered by UBSan
  separately to avoid the fixture's incomplete ASan longjmp stack context.
- slice-copy-jit.py under -O: 230,136 cumulative checks across both backends
  and layouts. Whole-memory and write-span oracles now also bound every VM read
  to decoded cells, metadata, the selected source bytes or the native return
  cell. Positive non-identical copies of at least eight bytes must perform an
  8-byte store. Source/index/output aliases, 256/257 boundaries, both overlap
  directions, exact tails and FP state are covered.
- array-create-jit.py: 178,638 AMD64 and 178,652 ARM64 checks. Representative
  leaves use 114/117 AMD64 units and 117/121 ARM64 units for raw/zeroed arrays,
  still within 128. array-create-layouts.py: 4,212,600 sizing/canary cases.
- Existing array-view-jit.py: 76,800 checks per backend. Its oracle now includes
  the native metadata-alignment guard. array-view-native.py: 7,104 C/RX view
  cases plus 20 root-shrink/copy/share/release cases. Existing array-create.py
  passes 208,896 map/stride/zero/alias/failure cases plus 576 retain checks;
  array-reuse-native.py passes 67,584 reuse cases plus existing owner/empty
  preflight. Heap bounds, Type retain, module-Type and complete-dispatch suites
  pass. Production xec.c compiles for both WORD layouts with implicit function
  declarations treated as errors.
- Sixty independent parent probes against 14ea0fe reproduce the previous slot,
  header, Type/root, C callback-publication and nil-source failures and reject
  byte-only copy kernels on both backends.

Run `python libinterp/tests/array-spans-native.py`. For its successful ASan
repeat, set `ARRAY_SPAN_NO_ERRORS=1` and
`CFLAGS="-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all"`.
The copy ASan repeat uses `SLICE_COPY_NO_ERRORS=1` with the same CFLAGS.
`ARRAY_SPAN_SOURCE` and `ARRAY_SPAN_SECTION` select C parent replay; sections
are constructors/views/slots/headers/absent. OP/KIND, COPY/HEADER and
CALLBACK_ONLY variants prefixed with ARRAY_SPAN narrow the matrices.
Guest parent replay accepts `ARRAY_EMITTER_DIR`, `ARRAY_ARCHES`,
`ARRAY_SPAN_OP`, `ARRAY_SPAN_KIND`; copy replay uses `SLICE_COPY_EMITTER_DIR`.

AMD64 and ARM64 remain at 173/173 complete JIT+C entries and 166/173 guarded
native paths (95.95%). These are opcode counts, not complete operand domains
or a finished VM port. ARM64 execution here uses Unicorn; physical ARM64 and
full 64-bit Inferno boot remain unverified. RV64 is deferred, and excluded
backends plus Tk/security/crypto are unchanged.
