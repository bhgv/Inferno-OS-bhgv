# Series0074: initial local-call control preflight

Source: `bbf60070d53c5586abe1d16b3bd57332af9c2a4c` on `experiment`.
Design reconciliation: `fce149a03e5fa9ad6395773f5d67504468261a5d`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41436152026

## Implemented boundary

The new private `include/dis64-local-flow.h` checks abstract control nodes,
not decoded or serialized Dis64 operands. It consumes the catalog's entry/end
intervals, revalidates their complete contiguous partition and checks every node,
including unreachable declarations. It refuses cross-function branches, invalid
fallthrough, interior call targets, unsupported opcodes and noncanonical unused
control fields. CALL requires an exact declared entry and a local continuation.

A deduplicated 512-byte call-edge bitset and bounded topological queue check all
functions without recursion or heap allocation. Cycles, including disconnected
cycles, refuse. Longest call depth must fit an explicit1..64 limit. Repeated call
sites do not inflate indegrees. Limits are64 functions and65536 instructions;
all refusals leave the caller's output summary unchanged. Intrafunction loops
remain legal; this does not prove termination.

## Independent checks

The Python oracle uses DFS instead of the C implementation's Kahn traversal.
Its1540 cases yield324 acceptances and1216 refusals. Cases cover all18 conditional
families, interval edges, backward/self targets, interior calls, malformed
partitions, repeated edges, disconnected cycles,64-function chains, exact and
exceeded depth, and65536/65537-instruction boundaries.

Five semantic removal controls separately weaken branch ownership, fallthrough,
exact call-entry matching, cycle rejection and depth limits. Each must fail an
active predicate with status10. Compilation failure, a signal or a sanitizer
report does not count as a successful control.

Local optimized UBSan passes all1540 cases and all five controls. The same gate
and controls are connected to `utils/tests/native64-port.py`, following classic
`mk` build and actual scalar console execution.

## Shared format decisions

FLASHVM-0063 explicitly accepts the initial export catalog, dedicated final
aligned eight-byte caller result cell, and single-prepared-frame/acyclic graph
subset. The proposal/catalog/status documents now record that response.

The result cell has a signature kind, not merely a width. Reuse invalidates its
old value; preserve needed results in proven numeric locals before another call.
Pending or unwound results cannot be read as valid. CFG merges must agree on
prepared identity/type, initialized arguments and result binding. Ownership must
retire exactly once on return, failure or abandonment. These rules still need
an executable state verifier; this checkpoint does not claim to implement them.

No IDs are allocated and no file CALL/FRAME/RET/LEA admission is enabled. Ordinary
Dis LEA is unchanged. New-format admission remains43 scalar opcodes; the private
control vocabulary is not permission to execute additional file instructions.

## JIT coverage

| Target | Native fast paths | Fast availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

These are guarded opcode-entry counts, not percentages of full VM completion or
new-format file admission. Complex allocation, scheduler and ownership work stays
in C. This checkpoint changes no native fast-path count. Shared file admission
is43/173, or24.86%; broader bytecode admission needs a complete verifier and
whole-file C/RX positive/refusal execution goldens.

## Remaining work before file calls

Compose decoded operand roles with catalog/control checks; verify prepared-frame
identity, argument stores, definite assignment, boolean values, exact result kind
and lifetime. Then agree whole-file positive/refusal goldens with FlashVM and
execute them through C/RX on native ARM64/x64 and RV64/QEMU before activation.
The Linux scalar consoles here do not establish Android APK/GUI startup or full
managed/imported application admission.

## Cross-target artifact checks

All three CI jobs passed: native ARM64/x64 and RV64/QEMU. The archives were
downloaded and their ZIP
SHA256 digests checked against GitHub metadata. Extracted emulators are ELF64
with machine183 (AArch64),62 (AMD64) and243 (RISC-V). All archives contain passing1540-case
flow logs plus all five controls and the628-case catalog preflight.

For each target, existing gates retain21504 fresh-message cases/16 controls,
5376 caught-handler cases/16 controls and896 terminal-call cases/four controls.
The241-source helper corpus executes482 whole interpreter/JIT processes; the
external FlashVM reader separately checks the emitted scalar files.

The packaged `procedures.dis64` is identical on all three targets:
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
These are the existing restricted-profile examples, not newly admitted calls.

All six packaged x64 examples (`loop`, `case`, `procedures`, each JIT0/1) were
replayed locally. Each reported its expected module, empty exception and empty
scheduler after retirement. ZIP metadata SHA256 and ELF machine/class checks
passed for every downloaded archive.

| Target | Execution | Result | Download |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [archive](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41436152026/artifacts/13136721850) |
| x64 | Native Linux, optimized UBSan C fixtures | PASS | [archive](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41436152026/artifacts/13138140865) |
| RV64 | Linux/QEMU | PASS | [archive](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41436152026/artifacts/13137021139) |

Artifacts use GitHub's14-day retention and include the console tarball plus
complete gate logs. These Linux archives are not Android APKs.
