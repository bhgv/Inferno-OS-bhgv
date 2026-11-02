# Series0081: raw linear caller setup bound to proved leaf returns

## Implemented

The new private whole-payload constructor derives canonical FRAME/argument MOV/
LEA/CALL/result-copy facts and strict ownership transitions from actual bytes.
Exact callee entry chooses signature; equal frame shape/type cannot substitute.
Every leaf body is freshly proved from the same metadata/code inputs before the
caller's numeric continuation proof. Final records/count publish atomically.

[Full contract, bounds and reproduction](dis64-bound-call.md).

The bounded graph is a linear module-entry caller plus noncalling helpers.
Helpers retain branches/loops and0080 typed return coverage. Multiple root calls,
all scalar argument/result kinds, prepared-pointer cells40/48/56, both argument
bitmap words and repeated typed argument stores are checked. New preparation
invalidates the old result; copied numeric locals remain initialized. Existing
ICVTWF is privately classified as an exact Int-to-Real producer for real-valued
positive call chains, without fabricating a seed or changing a runtime handler.

This does NOT execute new-format calls or prove dynamic failed-call cleanup.
Nested calls, general caller CFGs, arbitrary DAG summaries, actual FRAME/LEA8/
CALL retirement/unwind/cache-mode integration and shared whole-file C/RX goldens
remain. Compiler declaration and immutable container provenance remain trusted
obligations. No production loader/reader, ordinary Dis semantics, wire identifiers,
JIT emitters or complex C handlers change. No proof cache is introduced.

## Checks

3338 raw metadata/code vectors (1790 caller cases plus1548 return vectors under
the stronger contract):387 accepted/2951 refused.29 active semantic controls
cover canonical identity, capacities/budget/atomicity, temp/frame/call pointers,
result binding source/destination/width, argument offsets/completeness/reads,
pending isolation, linearity, stale ready state, result kind/local write, actual
leaf guarantees/address and exact conversion source/destination kinds.

Local optimized nonrecovering UBSan passes. Workspace is104 bytes/node on LP64:
40 decoded+60 numeric+4 queue,6.5MiB at65536 nodes, plus bounded stack/output.
Source: `0865f20f5b11fd5e7dfa15eb41b74f72c914966f` on `experiment`.
CI: https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41498541273

All three jobs pass: native Linux ARM64/x64 and RV64/QEMU. Each target passes
3338 call-composition cases (387 accepted/2951 refused), all29 active semantic
controls and prior1548/26,611/14 and2029/13 gates. x64 C fixtures use optimized
nonrecovering UBSan. Every target reports40 decoded+60 numeric+4 queue bytes/node.
Older17544 operand cases,4005 CFG graphs,98032 state records,1540 flow cases and
628 catalog cases remain green. Logs retain241 helper sources/482 whole C/JIT
processes,21504 fresh-message cases,5376 caught-handler cases and896 terminal-call
cases per target. This is our target evidence, separate from peer replays.

All ZIPs were downloaded; SHA256 matches GitHub metadata and consoles are ELF64
machines183/62/243. ARM/RV archives contain173 logs each, x64172. Packaged x64
loop/case/procedures replay in six JIT0/1 processes with expected module, empty
exception and empty scheduler after retirement. These are the existing scalar
images, not shared new-format call/LEA8 execution goldens.

FLASHVM-0069 remains the latest incoming review at this source checkpoint.
Its0079 owning-source replay and hosted metadata/file-wait work remain separate
peer evidence; no scheduler/descriptor adapter is imported into Inferno.

## JIT coverage (unchanged)

| Target/profile | Native fast paths | Availability | Complete JIT plus C |
|---|---:|---:|---:|
| ARM64 |166/173|95.95%|173/173|
| x64 |166/173|95.95%|173/173|
| RV64I |87/173|50.29%|173/173|
| RV64I + Zmmul |90/173|52.02%|173/173|
| RV64M |103/173|59.54%|173/173|

New-format file admission remains43/173 (24.86%), distinct from opcode-entry JIT
coverage and complete application readiness. This is a private verifier/Linux
scalar-console checkpoint, not an Android APK or full managed shell/GUI release.

## Downloads and archive verification

| Target | Execution | Result | Archive |
|---|---|---|---|
| ARM64 | Native Linux | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41498541273/artifacts/13168143296) |
| x64 | Native Linux, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41498541273/artifacts/13167549517) |
| RV64 | Linux/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41498541273/artifacts/13168252688) |

| Target | ZIP SHA256 |
|---|---|
| ARM64 | `84c962d0243c43e511d8dfb03997d27847fea77b872738ade8d68521e13c945d` |
| x64 | `e8643666a7bba5d338ef0c933ef72cde4ed28db877771e1c06c42952db95e7c4` |
| RV64 | `3f3e24e12c843cb925098230ec85ec83033166df8fe08ce24df3b9c2f0d4f456` |

All targets retain the same procedures.dis64 SHA256
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.
This emulator normally ends via SIGKILL after the verified retirement markers;
exit status alone is not the packaged replay predicate. GitHub retains archives
for14 days. They contain Linux consoles, existing scalar examples and gate logs.

Remaining before executable file-call activation: general caller CFGs/non-leaf
DAG composition, actual FRAME/LEA8/CALL runtime and cache-mode integration,
normal/error/unwind retirement and shared whole-file C/RX positive/refusal goldens.
The private input declaration/container provenance and full-input proof-cache
obligations remain explicit; no untrusted kind table or cache is introduced.

[Inferno outgoing journal](dis64-inferno-journal.md) and
[FlashVM incoming journal](dis64-flashvm-journal.md) retain direct coordination.
