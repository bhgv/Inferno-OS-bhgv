# Native64 series0083: trusted LEA8 in C instrumentation

This series repairs a dispatch inconsistency in the trusted owning-source
prototype from series0082. `xec` recognized `Module.cstep == native64lea8step`,
but debugger and coverage loops decoded LEA and called its ordinary WORD-sized
C handler. On LP64 with WORD32 this truncated a result address.

`vminstrument` now intercepts only LEA with that exact trusted callback identity,
before decoding. A successful callback publishes the next PC once. All other
instructions retain decode followed by their authoritative C handler. The helper
does not invoke arbitrary AOT callbacks or compose incompatible translation
profiles. Both `dbgxec` and `cpxec` use it. Ordinary LEA remains WORD-sized.

Debugger LOAD notifications still receive the decoded source slot captured before
the C handler. The coverage bucket is updated before execution, without reading
the old profile record after a callback. Existing stop, return, breakpoint and
child-notification code remains in place.

## Validation

The result-call fixture can now extract and execute the actual `cpxec` definition
with active isolated profile storage. VM heap, decoder, FRAME/CALL/RET, typed
result stores, LEA8 and cleanup remain production bodies. SPAWN is absent and
its scheduler adapters deliberately fail if reached.

Each target runs the original ordinary C/native-JIT matrix, then two additional
C instrumentation matrices: the unchanged coverage executor, and a helper probe
using the debugger's non-null source-output convention. Each matrix contains
10,080 accepted WORD32 executions and 96 preallocation refusals; 48 address-width
probes and 264 shape/fallback probes run in each of WORD32 and WORD64. Cached JIT
maps are present in half the instrumentation runs but their native entries are
deliberately bypassed. These are not additional RX executions.

A separate LOAD probe makes a test handler replace `REG.s` and checks that the
notification still receives the original decoded slot. The priority control
disables only the shared interception and must report the exact result-address
predicate with status 10 for JIT-cache absent/present. Signals, compilation
failures and sanitizer errors are not accepted as successful controls.

This does not execute an interactive debugger session or its full controller,
breakpoint, LOAD and SPAWN machinery. Production wiring is checked against both
extracted executor definitions; source-output and LOAD semantics are exercised
through the common helper. It is not a GUI/Android validation.

Local optimized nonrecovering UBSan x64 runs pass: the ordinary matrix, actual
coverage loop, source-output helper and both shared-priority control executions.
VM-address checks and a fresh emitter availability recount also pass.

[CI run41515225294](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41515225294)
passes all three targets at tested source
`fe72e2a3ca673322c9750b86e1569334be456d4c` (implementation `de4a1e8`, followed
only by restoration of the existing test driver's executable mode). ARM64/x64
execute natively; RV64 executes under QEMU. The x64 owning C fixtures use optimized
nonrecovering UBSan. This series makes no new whole-runtime ASan/TSan claim.

Each target passes both new 10,080-run instrumentation matrices, their width/shape
probes and two priority-control executions. The original C/RX matrix and all seven
of its active controls remain green. Prior 3,338 call-binding, 1,548 return,
2,029 initialization, 17,544 operand, 4,005 CFG and 98,032 state records, plus
21,504 fresh-message, 5,376 caught-handler and 896 terminal-call runs per target,
remain green. Those older handler suites do not establish new result-call
caught/resumed readiness or parallel heap mutation. External reader checks pass.

Downloaded all three archives and verified their ZIP SHA256 against GitHub's
artifact digests, ELF64 machine IDs183/62/243 and the new logs. ARM/RV have181
logs each; x64 has180. Six packaged x64 loop/case/procedures JIT0/1 processes
complete with expected modules, empty exception and empty scheduler after
retirement. The common `procedures.dis64` SHA256 is unchanged:
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.

## Downloads

These are experimental Linux scalar consoles and complete gate logs, not APKs
or a full managed GUI/shell. GitHub artifact retention is14 days.

| Target | Result | Archive |
|---|---|---|
| ARM64 native | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41515225294/artifacts/13176395665) |
| x64 native, UBSan C fixtures | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41515225294/artifacts/13177101928) |
| RV64/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41515225294/artifacts/13174953802) |

| Target | ZIP SHA256 |
|---|---|
| ARM64 | `d96cc25be304070d2a3978e6ed80d435e6464e73e9bc9f0d555f09ba6a3f1d27` |
| x64 | `67dbe5a90fda0e3b3ba385beb13c5aa368c3e433e89a8e9fbba7f73dde696356` |
| RV64 | `b5249acac94f49929efe62a3359b70be7fe26a9833f196c42b2f6dc1aa76f4df` |

## Remaining boundary

Trusted mode assignment still requires a fresh owning module, or quiescent
set/reset under owning scheduler/heap exclusion. There is no concurrent setter,
general AOT composition or file-loader activation. The function pointer is not
container identity or a reusable proof certificate.

Direct native leaf/complete entries still require explicit mode enforcement or
refusal. Caught recovery, general caller CFG/non-leaf graphs, immutable
container/declaration binding, and whole-file out-of-line C/RX goldens remain
prerequisites for production file CALL/LEA8 admission. The existing 43-op file
gate and wire format/IDs are unchanged.

| Architecture | Native fast path | JIT including C handlers |
|---|---:|---:|
| ARM64 | 166/173 (95.95%) | 173/173 (100%) |
| x64 | 166/173 (95.95%) | 173/173 (100%) |
| RV64I | 87/173 (50.29%) | 173/173 (100%) |
| RV64 + Zmmul | 90/173 (52.02%) | 173/173 (100%) |
| RV64 + M | 103/173 (59.54%) | 173/173 (100%) |

These opcode counts are not percentages of complete port or file-format readiness.
They describe published entry availability; operand guards can still select C.
The seven ARM64/x64 commands without a separate fast path are `IALT`, `INBALT`,
`ISPAWN`, `ILOAD`, `IMSPAWN`, `IEXIT` and `IRAISE`. They remain available through
the complete JIT-to-C dispatch. Replacing their scheduler/loader/exception logic
with native emission is not required for complete opcode support.

## FlashVM coordination

FLASHVM-0073 at `5e0768053eff9001ae9b185dea66232a30f6124f` received this
integration concurrently. Its execution evidence remains the earlier0082
prototype: receipt is not agreement on or replay of the new instrumented routes.

Reviewed its proposed shared-heap locking model at FlashVM commit
`befef7d850f68906f8741920c04b07bba6ce3527`. Agreement is with the design direction,
not parallel-mutator activation. Inferno still has global `REG R`, non-atomic
Heap/Type reference counts, direct JIT retain/mark writes and incremental GC
state. Heap locks alone do not make those paths parallel-safe. The reply in the
Inferno journal requests explicit reference transfer/publication rules, published
roots while parked, whole-cycle GC synchronization and context/cache ABI fencing.
No Heap layout, atomics, serialized format or vendored snapshot changed here.
