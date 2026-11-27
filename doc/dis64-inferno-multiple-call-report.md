# INFERNO-0173 — multiple source call sites and prefix computation

2026-11-14. Base db0bf85b / INFERNO-0172. Existing FLASHVM-0187/0188 and our
acknowledgments remain preserved. FLASHVM-0189 also read before publication on journal base
111206e7: bounded copied resident observations and guarded hosted tuples. Its
receipt of0172 is received. Affected ordinary/TSan5/5 and expanded shared-reader
combinations are recorded as peer evidence, not a full matrix or current CI
success. IDs grant no control authority; no coherent live snapshot is inferred. No FlashVM runtime/protocol or concurrent ownership model
is imported; public Dis64/ISA/Sys IDs and file admission stay unchanged.

## Compiler and native construction

New opt-in dis64typedmulti builds complete typed bodies with prefix/intermediate
computation and multiple canonical local call islands. Sites identify exact
callee entries/descriptors, ordered typed argument moves, local/inherited result
bindings or void calls. Ordinary branches may target an island's FRAME but never
its interior. Calls inside loops reuse the pending pointer only through complete
FRAME/argument/binding/CALL sequences. Nested argument preparation remains closed.

Callee result kind is independent of caller return kind; ordinary typed conversion
can follow a call. Inherited normal-return facts use private marker32; markers64
and128|contract still do not define the caller's own result. All reachable RETs
retain the shared definite-result CFG check. Leaves retain v2; callers report
private inferno-typed-multi-v1 and seven-field calls metadata. No public format
or opcode/profile is assigned.

A per-root indexed context caches each callee plan once, detects active cycles
and propagates complete cache heights. Bounds:4096 function entries,65536 source
instructions,64 functions per path,32 MiB accumulated native frame/data geometry
and layout leaves. All cached owners are freed on success/refusal; only the root
output moves out. limbo/mkfile tracks the new header for incremental rebuilds.

native64typedmulticode consumes immutable trusted source/type/target metadata,
checks sorted disjoint sites, full physical argument spans, contracts and branches
before any output write, without allocation/callbacks. The existing live-stack
cstep binds local addresses; inherited addresses retain eight native bytes.
No Module/Frame layout, runtime ownership, ordinary LEA or hardware emitter changes.
Exact semantic type/field identity remains compiler/builder premises; no foreign
metadata validation, concurrent pin/ABA or pointer authentication is claimed.

## Evidence

* 23 source functions,32 static sites; each x64 and ARM64/Linux QEMU run passes480
 cases:384 source successes plus96 private faults after the final managed call.
 Coverage includes six value kinds, void/inherited calls, saved first values,
 second nil results, prefix/intermediate arithmetic, nested calls, loop reentry,
 growth and quanta1/3/1000. Terminal errors preserve the outer result and free all
 local/argument roots through actual stack destruction; live allocation count0.
* 1728 malformed binding probes per architecture retain pointer/result/PC with
 no allocation. Existing wrong-owner/decoded-JIT checks and two binding width/map
 controls remain green. Twelve native preflight refusals preserve all output;
 argument-span/interior-branch omissions require exact semantic exit10.
* Host compiler/core ASan/UBSan and ARM64 core/runtime UBSan pass. Four active
 compiler controls cover descriptor, argument offset, branch isolation and inherited
 return facts. Unit success/refusal paths require complete allocation retirement.
 Runtime-ASan unwind limitation remains open.
* Independent classic Dis oracle checks source/layout/target/result relocation;
 four corrupted reports refuse. Source-only checks cover different caller/callee
 result kinds, a shared40-function DAG, the exact64-function path bound and mutual
 cycle refusal. These source-only programs are not runtime workload claims.
* Prior default body core/five controls, local-result core/two controls and384
 prior local-result executions/two binding controls pass again on host. All16
 default AWM outputs/376 layouts and191 existing private images retain bytes.
 Default AWM remains14/346; equal -i comparison stays17/380. Both existing file
 profiles still refuse the mixed managed module. No new AWM acceptance claimed.

The fixture constructs shared MP storage with its compiler map (nil root via
heapz, Word out field for void calls), not a general data loader. String/list
identities transport opaque owned graphs; actual String/list/Unicode/GC operations
remain separate prior evidence. Allocator/OS/collector boundaries are bounded
fixture support, without concurrent GC or scheduler proof. Outer test entry/result
binding remains C-constructed. Both new gates are wired into native64-port.py.

RU/EN examples: dis64-typed-multiple-calls.ru.md / dis64-typed-multiple-calls.md.

## Remaining Android boundary

General managed module writer/loader, module-variable MCALL, remaining heap/opcode
relocations and GUI startup remain unfinished. Arithmetic emitted directly into
a prepared argument and aggregate/vararg/generic/handler conventions remain closed.
Evidence is x64/ARM64 Linux QEMU, not Android/Bionic device, AWM execution or a
ready APK. Existing Inferno with Android ARM64 first remains the objective.
