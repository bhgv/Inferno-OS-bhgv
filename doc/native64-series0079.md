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

Source: `82d7bd954447686e59797159c172ca2dc00444e5` on `experiment`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41480077419

All three jobs pass: native Linux ARM64/x64 and RV64/QEMU. Each target passes
611 bound cases (227 accepted/384 refused), all14 active controls and prior2029
numeric cases/all13 controls. x64 C fixtures use optimized nonrecovering UBSan.
Downloaded ZIP digests match GitHub metadata; extracted consoles are ELF64
machines183/62/243. ARM/RV archives contain116 logs each; x64 contains115.
Earlier17544 operand cases,4005 CFG graphs,98032 state records,1540 flow cases
and628 catalog cases also pass. Logs retain241 helper sources/482 whole C/JIT
processes,21504 fresh-message cases,5376 caught-handler cases and896 terminal-call
cases per target. These are target results, separate from FlashVM owning replay.

An optional local ASan run could not complete because LeakSanitizer cannot read
/proc task state in this environment, including a repeat with leak detection
requested off. It is not counted as a successful ASan/memory verification.

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

## Downloads and packaged replay

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41480077419/artifacts/13159488591) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41480077419/artifacts/13161435354) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41480077419/artifacts/13160082664) |

All three ZIPs were downloaded and their SHA256 digests verified against GitHub:

| Target | ZIP SHA256 |
|---|---|
| ARM64 | `6e33272fb2294a3ceba3a7c570835dc64e76aabfe26d7e11881c3edcaf7a6e33` |
| x64 | `c1fac1937c0d0bacc74a892b7147343044936b874fc23512f131e60ce23b79d2` |
| RV64 | `4da9060654b93336d4f4a6d3f90229bf4747380e768c8ae3cc98f6861bc10c95` |

Packaged x64 loop/case/procedures replay locally with JIT0 and JIT1: all six
report the expected module, empty exception and empty scheduler after program
retirement. This emulator's normal shutdown uses SIGKILL after those markers;
status alone is not the acceptance predicate. All targets retain identical
procedures.dis64 SHA256
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.

Archives contain the experimental Linux console, existing restricted scalar
examples and complete gate logs; GitHub retains them for14 days. No Android APK,
full managed shell or executable new-format file calls are implied.

[Inferno outgoing agreement journal](dis64-inferno-journal.md) and
[FlashVM incoming agreement journal](dis64-flashvm-journal.md) remain the direct
coordination channels; only the Inferno-owned outgoing journal is written here.
