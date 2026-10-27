# Heap construction and module-clone prefix/span checks

`heap`, `heapz` and `heaparray` now reject a wrapping or unaligned native Type
prefix before reading descriptor fields or allocating. These mutable descriptor
owners retain the native alignment required by Type reference counts/callbacks.
`arraycpy` checks the complete source Array header and backward Heap origin
before reading its source reference count; its protected copier checks Type
prefixes and root Heap identities before metadata reads.

`newmp` treats its descriptor as read-only metadata: size and pointer-map counts
are copied through bytes, and map bytes use byte addresses. Packed descriptors
therefore work without typed C alignment assumptions. Full positive-width source
and destination spans are checked before copying bytes or initializing mapped
owner slots. Mapped child owners must have valid backward Heap origins and
native alignment before their metadata is consulted.

H, nil, dead/saturated owner behavior, pointer-map order, nested independent
array clones and partial-result cleanup retain their previous contracts. Empty
and zero-width array copies remain valid without backing data. Type/map metadata
must remain stable throughout cloning; this is not arbitrary pointer mapping
validation or a general mutable-Type contract. Descriptor-prefix alignment for
heap construction differs deliberately from byte-safe read-only map inspection.

## Verification

```sh
python3 libinterp/tests/heap-preflight.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/heap-clones.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/clone-owners.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/array-copy-source.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/record-slots-native.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/array-view-native.py
```

The new driver uses ASan/UBSan and NDEBUG by default and borrows real constructors,
clone helpers, destructors and VM error frames from the existing clone fixture.
Its 14,792 cases cover WORD32/WORD64 with native eight-byte pointers:

| Family | Per scalar model |
| --- | ---: |
| Three constructor descriptor prefix/alignment guards | 165 |
| Array source, Type and root owner preflight | 159 |
| Packed map descriptors, data/output shifts and exact owner/mark effects | 6,144 |
| Mapped data and descriptor span rejection | 288 |
| Bad first/later child with cleanup-safe partial results | 640 |

Early prefix/span rejection must allocate nothing and preserve data. Valid map
copies compare full descriptor/source/output images, canaries, reference counts,
mark colors and nprop. Later child failures retain only previously acquired
owners, and real freeptrs must release those exactly once. Oracle storage is
stable across error jumps and unrelated families run in separate processes;
ASan is not disabled. The inherited heap-clone fixture now keeps its ownership
predicates active under NDEBUG as well.

The unchanged clone contracts also pass 189 nested/module-allocation cases per
model, 1,280 mapped-owner cases plus six bad-map cases per model, and 17 source
array cases per model. Actual AMD64 constructor execution passes 84,816 C/fast/
complete-RX routes per model, plus 102 rejected routes in WORD32 and 126 in
WORD64. Array view tests pass 3,552 C/RX cases and ten root-copy/lifetime cases
per model. The record fixture runs its single VM test thread on a tracked
pthread stack, allowing ASan to restore shadow state after longjmp even when
main-stack discovery through proc is unavailable; instrumentation stays active.

Ten independent old-source controls fail: all five families in both scalar
models. They reach UBSan alignment/span diagnostics or SIGSEGV, rather than
fixture compilation failures. Replay with `HEAP_CLONE_SOURCE`,
`HEAP_NEWMP_SOURCE`, `HEAP_CLONE_MODELS=native|word32` and
`HEAP_PREFLIGHT_CASE=constructors|sources|mapped|spans|children`.
The new driver is included in the core regression suite.

This series changes common allocation/cloning paths used by JIT C fallbacks.
Opcode availability remains 173/173 complete entries including C and 166/173
native fast paths (95.95%) per AMD64/ARM64 backend. Full native ABI work and a
physical ARM64 run remain unfinished. No RV64-specific, MIPS/SPARC/PowerPC JIT,
Tk or security/crypto changes are included.


## Incoming synchronization

Publication includes incoming `320e658`, which moves Regex's internal syntax
and end markers outside the Unicode scalar range and adds hosted regressions.
Those five incoming files are preserved unchanged. The ten staged heap/test
images were checked against the pre-sync staged blobs and remained identical.
The incoming Python driver passes syntax compilation; its optional integration
requires a matching FlashVM NC64 compiler/runtime pair, unavailable in this
workspace, so that execution is not claimed here.
