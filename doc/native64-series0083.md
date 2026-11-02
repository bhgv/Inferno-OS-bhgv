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

Local optimized UBSan x64 validation and the three-target CI results will be
recorded after the complete source commit has been tested.

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
