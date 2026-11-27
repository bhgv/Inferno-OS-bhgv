# Series0080: actual typed result stores on every returning path

## Implemented boundary

The new private constructor binds actual existing MOV stores through `0(32(fp))`
to canonical result signatures and numeric incoming states, then checks every
entry-to-RET path. Numeric local writes never substitute for result stores.
Source initialization, exact kind and Bool invariants remain mandatory. Loops
are analyzed without claiming termination; nonvoid functions conservatively
require at least one reachable RET. Final summaries publish atomically.

[Contract, preconditions, budgets and reproduction](dis64-bound-return.md).

No executable CALL/FRAME/RET/LEA activation, general pointer/indirect memory,
format IDs or production reader/ordinary Dis semantics change. In particular,
valid incoming result-address provenance is still a checked-caller obligation;
this result is a conditional callee-body proof, not an execution capability.
Trusted compiler local provenance and actual container binding remain explicit.
The remaining composition is prepared-frame ownership, argument/result binding,
retirement/unwind and shared whole-file C/RX positive/refusal goldens.

## Checks

1548 raw metadata/body cases (937 return cases plus611 prior catalog cases under
the stronger contract):388 accepted/1160 refused. All scalar kinds, same-width
wrong kinds, source initialization, pointer base/displacement, all256 mode bytes,
branches/loops, unreachable malformed bytes versus valid unreachable reads,
128-slot state, late refusal guards,64/65 functions and65536/65537 instructions
are covered. Independent numeric TOP/intersection and boolean TOP/intersection
proofs check the C numeric FIFO plus cut-graph traversal.26 active removal
controls must fail semantically under optimized nonrecovering UBSan.

Source: `4573f9e7091824ba0aed3c476c8c006cce313589` on `experiment`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41488667198

All three jobs pass: native Linux ARM64/x64 and RV64/QEMU. Each target passes
1548 return cases (388 accepted/1160 refused), all26 active semantic controls,
earlier611 bound numeric/14 controls and2029 numeric/13 controls. x64 C fixtures
use optimized nonrecovering UBSan. Older17544 operand cases,4005 CFG graphs,
98032 state records,1540 flow cases and628 catalog cases remain green.
Logs retain241 helper sources/482 whole C/JIT processes,21504 fresh-message
cases,5376 caught-handler cases and896 terminal-call cases per target. These
are target executions, separate from FlashVM's owning-source replays.

Every ZIP was downloaded and SHA256 matches GitHub metadata; extracted consoles
are ELF64 machines183/62/243. ARM/RV archives contain143 logs each, x64142.
Existing packaged x64 loop/case/procedures replay in six JIT0/1 processes with
expected module, empty exception and empty scheduler after retirement. These
remain the restricted scalar examples, not new file-call return-store goldens.

FLASHVM-0069 independently replays0079:611 cases/all14 controls. Its replay and
hosted native metadata/directory-wait work remain separate peer evidence; no
scheduler/file adapter is imported here. Its proposal explicitly prefers a
reachable normal-return witness for the initial nonvoid subset; we agree and
implement that conservative private policy here without allocating wire IDs.
Actual caller ownership, result invalidation/unwind and bottom-up call summaries
remain before shared whole-file C/RX activation.

## JIT coverage (unchanged)

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

New-format file admission remains43/173 (24.86%), separate from opcode-entry JIT
coverage and from complete application/runtime readiness. Complex handlers stay
in C. This checkpoint does not deliver an Android APK or full managed GUI shell.

## Downloads and archive verification

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41488667198/artifacts/13163837230) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41488667198/artifacts/13163739068) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41488667198/artifacts/13163487670) |

| Target | ZIP SHA256 |
|---|---|
| ARM64 | `dbef495ab6f57b2c5625390a90c40494ce5870fa733ac25d60384c65d1f731d7` |
| x64 | `21548c0d22931a1aacc7664bca0cc46a9cf806968f0e22258634afabd5c9f3ca` |
| RV64 | `387f222e5e158136092fd40237bf5e9817719506eccaaf08ffb585c99333227b` |

All targets retain the same portable procedures.dis64 SHA256
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
For this emulator, normal shutdown uses SIGKILL after the expected retirement
markers; status alone is not the packaged replay acceptance predicate.

Archives include Linux consoles, existing scalar examples and gate logs;
GitHub retains them for14 days. The remaining checked caller binding, ownership,
invalidating operations/unwind, explicit LEA8 execution/cache mode and shared
whole-file C/RX call goldens are required before activation. No proof cache is
introduced; any future cache must bind all immutable validated input identities.

[Inferno outgoing journal](dis64-inferno-journal.md) and
[FlashVM incoming journal](dis64-flashvm-journal.md) retain direct coordination.
