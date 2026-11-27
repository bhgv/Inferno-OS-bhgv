# INFERNO-0197 — local managed results and forwarding

2026-11-15. Base61858e11 /0196. Selected Command helpers can return String,
list-of-String, ADT references and byte/int arrays, including nil and owning
slices. The existing two Context/list parameters remain fixed. Managed results
are ordinary existing runtime stores into mapped caller cells; public Command
init is still void, and no public managed signature kind or wire field is added.

## Result and lifetime contract

A normal local call adds the existing indirect LEA before CALL. Its source is
an exact mapped caller root; destination is the prepared frame's unmapped FP32
address cell. The constructor selects the existing native binder's contract24
(eight-byte managed result) rather than scalar8. The binder checks the full
caller span/map, stack/extension geometry and prepared frame before publishing
sizeof(pointer). Existing values need not be nil: normal managed stores retain
the new owner and release the old one. Scalar imported results retain contract8.

`return next(ctxt, argv)` can instead copy the current FP32 address to the
prepared child's FP32 with MOVL. This inherited address is admitted only inside
an exact result-bearing call packet and from a result-bearing current function.
The ancestor root remains in a live mapped frame throughout the call chain.
No general read/write of frame headers, bare address argument or pointer DATA
is enabled. Existing C/JIT CALL/RET and result binding execute these instructions.
The native binder itself and JIT emitters need no change.

The reader first discovers entries, result arity and the complete call graph.
Call sites of a helper must agree on binder presence. Cycles and unreachable
frames refuse. Result-bearing callees are proved before their callers, deriving
classes from checked stores/operations rather than accepting a signature asserted
by a call site. Void dependencies need no result-class ordering. Nil-only results
use the existing unknown-root class; concrete String/list/Context/array classes
must unify with all return paths and caller consumers. Byte/int arrays remain
distinct. Direct HEADP/TAIL returns carry their checked list/String classes.

Return assignment has its own definite-flow obligation, reusing the library's
otherwise unused LOAD-attempt fact: every normal RET in a result-bearing body
requires an executed result store or successful inherited-result call. Loops do
not invent a first assignment. Result stores use only the FP32 borrowed address
and zero displacement; the source must be an initialized mapped/readonly root.
Root init cannot store through an unbound result address. Other helper parameter
shapes, scalar/aggregate results, recursion and helper imports remain refused.

## Evidence

* 268 source pairs, including30 new result pairs, each through96 scheduler roots
 on native x64 and ARM64/Linux QEMU with actual collector epochs, C/JIT caller
 and provider combinations, stack growth and retirement/error paths. New cases
 cover Unicode/NUL, nil/empty, list head/tail, Context identity, byte/int arrays,
 backing slices, replacement of an existing owner, loops, nested forwarding and
 live caller values across a call.
* 1048 main roots per rebuilt complete x64/ARM64 console plus missing-file,
 hash/default-policy/entryless controls and revised identical RU/EN tutorial.
 The example now calls a String-returning local helper in C and RX.
* 145 independent valid/malformed result images per target; host raw proof uses
 ASan/UBSan. Return-definition and concrete-class omission controls fail with
 the expected semantic mismatch, without executing malformed images. Binder
 slots/maps/offsets, inherited headers, wrong classes and branch entries refuse.
* An independent GC observer requires the full result address above4GiB to match
 a mapped root in another live stack frame. Forwarded addresses may reach an
 ancestor. Omitting only a recorded caller result root loses a fresh returned
 list after callee retirement; the observer stops before resuming. Changing the
 native binder from managed24 to scalar8 trips its map guard before publication.
 Both controls pass on x64 and ARM64. Observer bookkeeping is reset at process
 retirement so recycled native Type addresses are not mistaken for live helpers.
* 536 deterministic selected files; previous238 pair/476 files,62 prior scalar
 clients and all classic outputs match0196 byte-for-byte. Existing local void,
 scalar-return, primitive descriptors, array/index, String, reference, Context,
 GC and constructor rollback gates are exercised alongside the result work.

Use dis64-command.py and dis64-command-boot.py for source/full-console execution.
dis64-command-result-proof.py also accepts `--control defined` and `classes`.
Runtime controls are `--case result-list --gc-control local-result` and
`--case result-list --local-result-map-control`. Producer comparisons use
`--result-baseline` pointing to0196's compiler. The existing ARM64 environment
and runner apply; no installed runtime dependency was added.

## Scope and peer receipt

Computed returns outside the selected opcode set can use an explicit local
before returning. General parameters, numeric/aggregate results in this local
profile, module imports/exports, handlers and arbitrary ADT payloads remain
separate work. The actual16-source AWM preflight still compiles classically and
refuses the bounded profiles. These Linux/QEMU results do not establish
Android/Bionic GUI/APK, whole-VM sanitizer/concurrent-GC or physical ARM64/RV64
acceptance. Android ARM64 remains the first application target.

FlashVM master remains c81c123fb68840ad64017fff1cf33e6d3d5dfb90 at receipt. Own
journal relays the local result/frame/root contract and evidence. Incoming
0202/0203 and the own prior journal prefix are preserved; no DisVM runtime,
concurrent ownership model, wire-kind assignment or vendor-pin change is imported.
