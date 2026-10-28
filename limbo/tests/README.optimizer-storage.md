# Optimizer storage, graph ownership and signed positions

This series fixes the storage side of the Limbo optimizer feeding the Dis VM:

- `bnew` rejects negative dimensions and cell-mode bitmaps whose padded indices
  exceed signed `int`. The last accepted cell still ends exactly at INT_MAX.
- `allocbits` rejects empty/negative row counts, oversized row-vector bytes and
  negative position counts before allocating the first row.
- `optpositions` validates instruction plus dummy-definition counts before their
  addition. Both use/def tables share the checked result; later dummy-definition
  and entry-flow positions stay representable.
- `inspc` and `len` reject counts before incrementing beyond INT_MAX.
- `leader` bounds block-vector bytes, saturates its final growth to the permitted
  capacity and rejects exhaustion before allocation, copying or publication.
- `findb` rejects leader indices above the owning vector's live count.
- `beq` rejects unequal row sizes before reading bitmap cells.
- `mkblocks` frees its temporary list sentinel. Previously every optimized
  function leaked one `Block`; the sentinel is not a graph predecessor.

No instruction encoding, Dis slot width, compiler frame ABI or optimizer order
changes. These checks do not make the complete LP64 VM ABI compatible.

## Verification

```
python3 limbo/tests/optimizer-storage.py
python3 limbo/tests/optimizer-bits.py
python3 limbo/tests/optimizer-graphs.py
python3 limbo/tests/optimizer-ranges.py
python3 utils/tests/native-bootstrap.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-native.py
BRIDGE_BACKEND=arm64 BRIDGE_PREFLIGHT_ONLY=1 python3 libinterp/tests/jit-dispatch-native.py
```

The new driver extracts real production structures/functions and executes 655,792
predicates across WORD32/WORD64 with ASan/UBSan and NDEBUG. Ordinary dimensions
cover every bit count through 4096 and leader growth from capacity 1 through 256.
Allocator-boundary probes observe exact maximum legal extents without allocating
large buffers. Rejected dimensions must fail before any allocation; separate
checks distinguish compiler rejection from an attempted large allocation.
Graphs with 1–129 instructions exercise actual block/edge construction and require
all tracked allocations to be released. The fixture counts ownership directly;
unavailable process-based leak scanning is disabled, not address instrumentation.
The sandbox may print ASan symbolization/longjmp warnings.

Six independent parent-source replays (`OPTIM_STORAGE_SOURCE`, selecting
`OPTIM_STORAGE_CASE=bitmap|rows|leaders|lookup|compare|ownership`) fail with the
previous `optim.c`: attempted invalid allocation, unguarded lookup/comparison,
or the retained sentinel. The positions helper is new; its boundary test is not
claimed as an independent parent replay. Instruction/declaration counts above
INT_MAX are guarded in source but not materialized as billions of input nodes.

Existing checks pass: bitmap tests report 16,415,642 predicates per host ulong
model; graph tests cover 1,582,284 small graphs plus deep walks and wide counts;
byte-range tests report 95,224 cases per WORD model. Native bootstrap builds
ELF64 mk/iyacc/decgen, regenerates the grammar/decoder and compiles all 17 Limbo
translation units.

Actual x86-64 JIT/C tests pass 177,152 routes, 6,183 compiler cases and 295 VM-loop
integration cases per WORD model. ARM64 compiler preflight passes 6,215 cases per
model; ARM64 instructions are not executed by this host test. Fresh readiness
measurement confirms 173/173 complete entries and 166/173 guarded native paths
on both priority architectures. The detailed command table remains
[README.jit-readiness.md](../../libinterp/tests/README.jit-readiness.md).

This series adds no native opcodes and does not claim a complete 64-bit boot.
Tk, security/crypto, RV64 and MIPS/SPARC/PowerPC JIT sources are unchanged.
