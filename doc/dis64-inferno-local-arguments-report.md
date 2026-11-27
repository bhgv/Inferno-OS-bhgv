# INFERNO-0198 — exact scalar arguments in local Command calls

2026-11-15. Basea5ad118d /0197. Selected local helpers now accept zero to eight
byte/int/big/real arguments, either alone or after the existing Context/ListString
prefix. No-argument helpers are supported. Existing void/managed results,
forwarding and per-body ownership checks compose with these call shapes.
Exported Command.init and executable client entry signatures remain unchanged.

## Private call packet and initialization

The existing typed multi-call lowering already maps declared arguments to native
callee offsets. The writer now admits the additional checked helper signatures.
No public kind, function hash, opcode, wire section or serialized signature tag
is assigned. The reader derives prefix/arity/kinds from complete FRAME, argument
stores, optional result binder and CALL packets. All callers of one entry must
agree on this tuple. Managed inputs remain only the known two-argument prefix;
an arbitrary MOVP parameter cannot assert its own pointee class.

Numeric parameters start at FP64 without the prefix or FP80 after it. Arguments
are densely packed with natural alignment: byte1, int4, big8, real8. Distinct
big/real classes remain exact despite equal widths. Scalar stores have no middle
operand, target the same prepared frame at caller FP40 and use the exact next
aligned offset. Byte immediates are bounded0..255; big/real arguments come from
initialized typed FP slots. Trusted immutable byte constants are folded with the
existing checked byte-operand helper before encoding; general MP DATA/stores
remain outside the profile.

Each callee body receives only those exact parameter spans as defined scalar
entry facts. Parameter slots cannot overlap roots, headers, reserved tail cells,
padding or each other. Numeric call-source slots participate in the persistent
per-body class table, not a discarded local copy. Ordinary definite-flow checking
still requires every source value on every caller path. An unused local and a
loop back edge cannot invent a parameter definition. Altering a callee parameter
writes its own frame, preserving pass-by-value behavior.

Helper frame descriptors may now be as small as80 bytes and need no Context/list
root bits when the prefix is absent. The root export still requires its original
mapped arguments. Every actual helper map and code span is independently checked;
recognized optional frames still need unique entries and a reachable acyclic
call graph. Stack construction, argument moves, native binder, C/JIT call/return
and cleanup are the existing runtime paths.

## Evidence

* 300 source pairs with real GC, each through96 scheduler roots on x64 and
 ARM64/Linux QEMU.32 new cases exercise empty calls, scalar values and immediates,
 signed limits, packed bytes, mixed/reversed eight-byte values, eight parameters,
 both prefix modes, mutation, loops, nested/forwarded managed results, heap
 objects live across calls and exceptions from helpers.
* 1176 main roots per rebuilt complete x64/ARM64 console plus missing-file,
 hash/default-policy/entryless controls. Identical RU/EN tutorials add an int
 parameter to the String-returning helper and execute through C/RX.
* 261 independent valid/malformed argument images on each architecture; host raw
 proof uses ASan/UBSan. Exact offsets, widths, source classes, pointer substitutions,
 mapped numeric arguments, short frames and entries into setup refuse. Removing
 numeric entry definitions rejects a valid input; omitting immutable scalar-kind
 checks admits the selected invalid input. Both controls fail semantically before
 any malformed code execution.
* Actual helper-frame GC marking omission detects a lost callee-private list with
 scalar arguments. Existing managed result-address/map/owner, local-cycle,
 descriptor/array/index/String, Context/reference, scalar return and constructor
 rollback gates remain exercised. Parameterless frames do not fabricate roots.
* 600 deterministic selected files. Previous268 pair/536 files,62 prior scalar
 clients and all classic outputs compare byte-for-byte with0197. Fourteen complete
 well-typed source refusals include unsupported standalone managed arguments,
 numeric results, recursive/importing helpers and scalar/helper count limits.

Reproduce with dis64-command.py and dis64-command-boot.py;
dis64-command-argument-proof.py accepts `--control seed` and `--control kind`.
The targeted collector control is `--case args-live-callee --gc-control local-frame`.
Producer comparisons use `--argument-baseline` pointing to0197's compiler.
Existing ARM64 cross-compiler/runner options apply; no external dependency is added.

## Frontier and peer receipt

General standalone managed parameters, local numeric/aggregate results, imports,
multiple exports, handlers and general ADT DATA remain outside this selected
Command profile. Other existing scalar-library profiles are unchanged. The real
sixteen-source AWM preflight still compiles classically and refuses the bounded
profiles. Linux/QEMU execution is not Android/Bionic GUI/APK, whole-VM sanitizer/
concurrent-GC or physical ARM64/RV64 acceptance. Android ARM64 remains first.

FlashVM master is still c81c123fb68840ad64017fff1cf33e6d3d5dfb90 at receipt.
The own journal reports this parameter/slot contract and evidence. Incoming
0202/0203 and the own prefix are preserved; no DisVM runtime/concurrent model,
managed wire-ID agreement or vendor-pin change is imported.
