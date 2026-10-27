# Packed pointer maps, GC and return-frame preflight

The earlier byte-safe mapped-field work did not cover descriptor reads: incmem,
freeptrs, scanptrs and initmem still accessed `Type.np` through typed pointers.
RET/MCALL could capture an unaligned descriptor safely and then trigger undefined
behavior in cleanup. markheap additionally loaded packed record fields through
`WORD**` and advanced that pointer even for zero map bytes.

All five helpers now read the map count through memmove and the bitmap through
byte addresses. markheap loads only marked native-pointer slots through memmove.
It preserves ascending traversal, repeated-owner suppression, H handling,
Setmark, visit/gce accounting, the recursion-depth limit and canonical child
Type callbacks. freeptrs retains its reverse order within each map byte. Empty
maps do not require a usable data address. Descriptor maps remain immutable
during a traversal; child Heap headers and callback Types retain their existing
canonical layout requirements.

RET in C, AMD64 and ARM64 now rejects a saved caller FP whose full frame-header
span wraps before reading an otherwise unused Type or publishing caller state.
The root-frame error, normal return and cleanup fallback contracts are retained.
These checks do not prove allocation membership or validate arbitrary map data.

## Validation

Run from the repository root; guest drivers require Unicorn and Capstone:

```sh
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/pointer-maps.py
python3 libinterp/tests/gc-maps.py
python3 libinterp/tests/module-transition-native.py
python3 libinterp/tests/control-transfer-native.py
python3 libinterp/tests/control-transfer-jit.py
python3 libinterp/tests/return-native.py
python3 libinterp/tests/return-jit.py
python3 libinterp/tests/stack-lifecycle.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

| Matrix | WORD32 | WORD64 |
| --- | ---: | ---: |
| Four real pointer-map helpers, independent record/Type byte shifts | 393,408 | 393,408 |
| Real GC masks, owner colors, visit budgets and initial depths | 1,474,560 | 1,474,560 |
| Real recursive GC chains/cycles and empty maps | 5,968 | 5,968 |
| Builtin MCALL through C/fast-decline/complete RX with real freeptrs | 49,152 | 49,152 |
| Existing LOAD and pointer-free builtin routes | 282,624 | 282,624 |
| Control C/fast-plus-C/complete RX full-image routes | 21,504 | 21,504 |
| Invalid control transfers | 800 | 808 |
| Native RET metadata/cleanup/register cases | 1,920 | 1,920 |

The four-helper matrix enumerates all map-byte masks, one/three-byte maps,
repeated child owners and H slots. It compares complete data and descriptor
images, references, mark colors, nprop and callback order. It now uses explicit
checks, so NDEBUG cannot remove its oracle.

The GC matrix independently varies descriptor/data alignments, preexisting
mutator colors, nil child Types, exhausted/partial/sufficient visit budgets and
the depth boundary. Graph tests cover lengths 1, 2, 7, 64 and 70, both terminating
chains and cycles, and budgets 0 through 73. They check exact callback order,
unchanged references, colors, budgets, gce and restoration of markdepth.

The mapped builtin matrix uses real native Heap owners, Frame/Type layouts,
actual freeptrs and destructor counts after the real MCALL C handler, including
complete generated JIT tails. Every frame/descriptor byte alignment, all 16
maps of four payload pointers, repeated owners and H are exercised. The separate
LOAD/builtin error matrices still pass. These three map/GC/builtin matrices run
with ASan/UBSan and NDEBUG; process-inspection leak checks are disabled as in the
existing fixtures.

AMD64 and ARM64 guest control transfers each pass 19,232 span/image/write-trace
cases; each isolated RET guest suite passes 5,504 additional cases. The malformed
caller-FP cases put the unused Type in unmapped storage, making late validation
observable. AMD64 complete dispatch still passes 177,152 routes, 5,002 compiler
and 295 integration checks per WORD model; ARM64 compiler preflight passes
5,002 per model. Native execution here was AMD64. ARM64 instructions were tested
in emulation; a physical ARM64 run remains outstanding.

## Parent probes

Eighteen independent probes reject parent 51227cb:

- Four helper families under each WORD model fail UBSan on a misaligned Type.
- GC descriptor and record misalignment are isolated under each WORD model.
- Mapped builtin cleanup with the parent heap.c fails under each WORD model.
- Parent C RET accepts a wrapping saved FP under both WORD models.
- Both parent RET emitters touch the unmapped Type before rejecting that FP.

Use HEAP_MAP_SOURCE, POINTER_MAP_CASE=inc/free/scan/init and POINTER_MAP_WORD_BITS
for helper probes. GC_MAP_SOURCE, GC_MAP_CASE=matrix, GC_MAP_WORD_BITS,
GC_DESC_ALIGN and GC_RECORD_ALIGN isolate collector cases. TRANSITION_HEAP_SOURCE,
TRANSITION_CASE=mappedbuiltins and TRANSITION_WORD_BITS isolate the C/JIT cleanup
boundary. CONTROL_SOURCE, CONTROL_SECTION=invalid, CONTROL_BAD=16 and
CONTROL_OP=12 isolate the C return; CONTROL_EMITTER_DIR and CONTROL_GUEST_OP=2
select the guest return probes.

## Readiness

AMD64 and ARM64 retain 173/173 complete JIT entries including C, and 166/173
guarded native fast paths (95.95%). This series closes runtime/GC alignment
gaps; it does not select a new Dis pointer-map quantum or resolve the native
compiler/frame/loader ABI described in
[README.native-bootstrap.md](../../utils/tests/README.native-bootstrap.md).
Neither a full 64-bit VM boot nor physical ARM64 acceptance is claimed. RV64,
Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources remain outside this series.
