# Series0079: canonical function binding for numeric Dis64 proofs

## Implemented

The new private catalog-bound numeric constructor composes raw exports/pool/types,
canonical function intervals/signatures and trusted compiler slot declarations
with the actual-byte operand and numeric fixed-point checkers. Exact identity
includes frame Type ordinal, not just equal layout. Global branch ownership is
checked before local rebasing; cross-function fallthrough and root/helper terminal
role confusion refuse. Function entry states and module-wide budget are isolated.
All final results publish atomically after every function passes.

The existing numeric solver kernel is shared internally; its standalone raw-byte
validation remains. No executable file CALL/FRAME/LEA admission or new format IDs
are enabled. Local kinds/provenance remain trusted compiler premises. Actual
nonvoid result stores/return guarantees and strict prepared-ownership composition
remain before shared whole-file C/RX call goldens. This is Linux scalar-console
work, not a new Android APK or GUI acceptance claim.

[Detailed contract and reproduction](dis64-bound-numeric.md).

## Checks

Independent raw metadata/code reference covers canonical identity permutations,
equal-shaped wrong frame ordinals, per-function argument seeds and state isolation,
global/local PC confusion, terminal roles, unreachable malformed instructions,
metadata truncation/trailers, exact/below aggregate budgets, late refusal/guards,
64/65 functions and65536/65537 total instructions. 611 cases yield227 accepted/384 refused. Fourteen active removal controls
must fail semantically. Prior2029 numeric/13 controls and older target gates remain.

Native ARM64/x64 and RV64/QEMU validation is pending at this source checkpoint.
No successful target execution is inferred from local optimized UBSan alone.

FLASHVM-0068 independently reviewed/replayed0078 (2029 cases/all13 controls).
This is peer replay evidence, separate from our target runs. Its hosted file FIFO
and managed worker work is not imported as an Inferno scheduler change.

## JIT coverage (unchanged)

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

New-format file admission remains43/173 (24.86%), a different boundary from
opcode-entry JIT coverage. Neither percentage is complete runtime readiness.
Complex handlers remain in C.
