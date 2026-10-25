# Descriptor reservation and metadata-only prefixes

The direct list, byte-array and module-link allocation paths now reserve
Type-counter capacity before allocating or publishing an owner:

- `cons` rejects dead/saturated Tlist and dead tails before allocating.
- `consp` reserves Tptr before cons can publish its new node.
- `consmp` reserves the payload Type before allocating. If it is Tlist, both
  the node descriptor and mapped payload need a retain, so INT_MAX-1 must
  decline and INT_MAX-2 is the last legal starting count.
- `mem2array` checks Tarray/Tbyte counters and the complete heap/header/payload
  size before nheap. Negative lengths keep their historical empty-array rule.
- `mklinkmod` checks Tmodlink before allocating its variable-length link table.

AMD64/ARM64 SLICEC now accepts any validated nonempty zero-start prefix in
unique ASCII or Rune storage, without the 128-character moving-copy limit.
All ownership, representation, capacity and full-WORD bound checks still run.
Success writes only the signed string length. C explicitly skips self-copy
for this case. Nonzero-start slices retain the existing bounded native loop
and C fallback for longer copies. No opcode ID was added in this checkpoint.

Tests include 36 list descriptor/tail/alias cases, 10 byte-array preflight
cases and four module-link counter cases per WORD model, in addition to the
existing constructor suite. The new drivers use production function bodies
and real interpreter declarations, check allocation counts and failure images,
and verify the final legal retain. Selected ASan/UBSan checks pass.
All 82 host drivers selected by --core pass, as do the native compiler
publication/fallback/metadata/retry/unmap checks. Parent-source replays fail
for list, byte-array and module-link regressions. Independently removing the
aliased Tlist reservation produces a UBSan signed-overflow report; removing
the dead-tail guard also fails its regression.

SLICEC emulation covers 27,648 complete image/write/FP cases per backend,
including INT_MAX-length prefixes. Actual C/published RX AMD64 checks also use
a large virtual reservation with inaccessible payload pages to verify that
prefixes do not read or copy the payload. Existing native string slice tests
continue to pass for both WORD32/native-pointer64 and WORD64.

| Backend | Guarded native opcodes | Interpreter only |
| --- | ---: | ---: |
| AMD64 | 134 | 39 |
| ARM64 | 134 | 39 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 I | 87 | 86 |

Counts exclude INOP and unused IECLR, for 173 active instructions. These are
partial guarded paths, not full operand coverage or complete native VM ABI.
ARM64 remains tested by instruction emulation rather than physical hardware.
