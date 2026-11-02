# Bounded scalar source procedures: through series 0065

`limbo -6` can specialize direct FRAME/CALL pairs into its sole exported
`init()` before the existing numeric-case and scalar lowering passes. This is
source inlining, not a Dis64 calling ABI. The agreed np0 executable profile still
admits exactly 43 of 173 Dis operations. No opcode, section, feature bit,
signature, coordinate schema or wire identifier is assigned.

The current bounds are 16 compiled functions including the entry, 32 original
instructions per helper excluding its final RET, and 128 bytes of legacy helper
frame extent. Each helper has no result, no recursion and no managed frame map.
Int/Big/Real arguments use validated consecutive scalar copies. Bounded acyclic
helper-to-helper calls are admitted. Numeric helper case tables are admitted
within the same eight-label bound as entry-local tables. Helpers may contain
scalar conditional branches, loops and early returns. Series 0058 also admits
user-declared int/big/real locals with fresh zero initialization per call.
Byte, pointer and aggregate locals remain refused. The entry can retain
its own loops, branches and bounded numeric cases around the calls.

The compiler validates exact FRAME/argument-copy/CALL setups, callee start and frame
descriptor identity, and matching caller frame-pointer temporary. Branches and
case arms cannot enter a CALL while skipping its FRAME. Flattening uses private
instruction and case-table copies; it leaves ordinary Dis output and original
compiler metadata untouched. Empty procedures become a jump to the next
instruction. Each helper's terminal RET disappears; the entry RET becomes EXIT.

Helper scratch and compiler temporaries occupy an appended region separate
from caller locals and the caller's scalar scratch tail. Separate spans at each active helper depth preserve ancestor locals.
Sibling invocations reuse their depth span. For helpers with declared locals, series 0058
renews the entire private span with MOVW zero stores before every call. This
covers scratch, padding and both halves of Big/Real storage; caller locals and
caller scratch stay separate. Helpers with compiler temporaries alone retain
the previous output without added stores. Unsupported shapes fail compilation rather than emitting
file CALL/FRAME or falling back to an incompatible ABI.

The series 0057 version of `utils/tests/dis64-inline.py` covered 18 accepted sources, each executed once
through the interpreter and once through JIT. It checks helpers before and after
the entry, empty bodies, repeated calls, loops, three numeric case forms, Big
wraparound, Real moves, live caller Big/Real locals, scratch isolation, both
accepted size boundaries, and faults traced back to helper source lines.
Nine valid ordinary-Limbo programs independently test refusal of unsupported
calling/storage shapes. Ordinary assembly must still contain FRAME/CALL;
ordinary Dis output must remain unchanged. A deliberately misplaced helper
scratch address tests the independent storage-window witness.

Local strict-UBSan host bootstrap, clean classic-mk ARM64 port gates, x64
execution and RV64/QEMU execution pass. ARM64 whole-process replay uses QEMU;
subsequent GitHub native ARM64 CI is separate hardware-backed evidence. The
external FlashVM reader accepts the images without a reader change. See CI
logs for complete runtime/GC/lease gates; this does not establish full shell,
imports, managed calls, Android startup or native RV64 producer execution.

The downloadable console bundles include `procedures.b`, `procedures.dis64`
and its private diagnostic map. Run `./emu -v -r "$PWD" -c 1 -d /procedures.dis64`
and repeat with `-c 0`. It checks 4097 calls, helper scratch, caller locals and a
numeric case; expected module is Native64Procedure.

JIT coverage remains distinct from file admission: ARM64/x64 have 166/173
native fast paths (95.95%); RV64I has 87 (50.29%), Zmmul 90 (52.02%), and M
103 (59.54%). All three retain 173/173 support through JIT plus C handlers.
Complex operations continue to use C where appropriate.

## Published validation and downloads

Source commit: [ab03187](https://github.com/bhgv/Inferno-OS-bhgv/commit/ab03187119dc96800ac05c1a41afce48969354d3).
[CI 41343429742](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742)
passes all three clean classic-mk ports: native Linux ARM64 and x64, RV64/QEMU.
Each archived helper execution log records 18 sources / 36 whole C/JIT
processes; the separate external-reader log records 18 images / zero execution
processes, with all nine refusal controls active. The existing 67-case gate
still runs 134 whole processes per architecture.

| Target | Console, three source/image/map examples and complete logs |
| --- | --- |
| ARM64, native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742/artifacts/13098273734) |
| x64, native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742/artifacts/13098770059) |
| RV64, QEMU CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41343429742/artifacts/13097759848) |

Artifacts require a GitHub login and expire according to repository retention.
All three archives contain byte-identical procedures.dis64, SHA256
`a07604d05c0d11efee54d2ae8fe874c5c9c5c7c467ae2bf4f6f7591c99bb2763`.
The downloaded ARM64 console additionally executes all three bundled examples
successfully with JIT0/1 under Linux/QEMU and an explicit ARM64 libc sysroot.
This replay is separate from the native ARM64 CI evidence above.

The owning native Limbo integration gate also passes 72 ordinary Dis/AOT source
cases and exact ABI32/late-link checks. The previous compiler produces identical
ordinary Dis for the scalar compatibility corpus. No APK or full shell claim
follows from these restricted console results.

## Series 0058: fresh scalar locals

The full updated helper gate has 27 accepted sources, 54 whole C/JIT execution
processes per architecture, two active stale-local execution controls and ten
valid ordinary-source refusal shapes. New cases cover repeated zero-based local
updates, mixed scalar widths, Real zero, explicit initializers, reuse after a
different helper dirties the region, calls inside numeric cases, and the exact
128-byte helper-frame boundary. A 136-byte scalar helper is separately refused.

The stale-local control preserves all file framing and valid scalar operands.
It replaces the second call's MOVW-zero stores with ORW-zero, leaving the first
call initialized and then preserving its stale contents. Both interpreter and
JIT must report the source's expected zero-divide check. The external reader
accepts this intentionally wrong program: format admission is not a proof of
source-language semantics. Ordinary compiler output remains unchanged.

Generated initialization instructions use the source call-site diagnostic span;
the copied helper body keeps its original helper span. No physical UTF-8 column
schema, debug section, signature or native callable witness is introduced.
The sole-entry fast path and temporary-only helper output stay unchanged.

General arguments/results, nested/recursive calls, managed storage and full shell
startup remain closed. This series advances the source subset on the same 43-op
file profile; it does not increase the native JIT opcode percentages.

### Series 0058 published validation

Source [11d4b3d](https://github.com/bhgv/Inferno-OS-bhgv/commit/11d4b3d92d5decf47a813a6b92ea809d0e129c0c)
passes [CI 41348167565](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41348167565):
native Linux ARM64/x64 and RV64/QEMU. The archived execution gate for each target
records 27 sources, 54 whole C/JIT processes and two active stale-local controls;
the separate reader-only step records zero execution processes/controls and
checks format admission, including the intentionally wrong stale-local image.
Ten calling/storage refusal shapes remain active in both invocations.

| Target | Console, source/images/maps and complete logs |
| --- | --- |
| ARM64, native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41348167565/artifacts/13100416617) |
| x64, native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41348167565/artifacts/13099539320) |
| RV64, QEMU CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41348167565/artifacts/13099424565) |

The renewed procedures example has 32 decoded instructions; its per-call local
counter must sum to 4097. The downloaded ARM64 console retires all three bundled
examples under Linux/QEMU with JIT0/1 and the explicit ARM64 libc sysroot.
This package replay is separate from native ARM64 CI and does not establish
Android or native RV64 compiler execution. The 18 prior helper programs remain
byte-identical against the series0057 compiler, and native Limbo's 72 ordinary
Dis/AOT/ABI32 checks pass. Existing GC, runtime, caller/field/lease and numeric
case gates also pass in each clean CI port.

procedures.dis64 is byte-identical across all three downloaded archives, SHA256
`ad1c2750a4955db7719797fedcf2647585cc07c19ae49727416b310ec7334c47`.


## Series 0060: control flow inside scalar leaf procedures

The compiler now accepts scalar helper conditions, loops, break/continue and
early returns. Each copied branch is checked against the original function's
half-open instruction span, then rebased relative to that particular call copy.
Early RET becomes a jump to the caller continuation. Branching to the terminal
RET has the same continuation target; it cannot terminate the whole program.
Per-call zero initialization precedes the copied body. Loop backedges target
the body, so they cannot repeat initialization and silently reset loop locals.

The existing 16-function, 32-instruction and 128-byte frame bounds still apply;
they bound specialization size, not execution time or loop iteration count.
Arguments/results, nested/recursive calls, helper case tables and managed locals
remain refused. The shared executable profile remains exactly43 operations.
No runtime file-call ABI or FlashVM reader change is required.

New whole-process tests distinguish correct caller continuation from both an
incorrect result and a premature VM exit: after successful self-checks, each
positive control-flow program deliberately triggers a caller-only invalid-shift
fault. Wrong results trigger zero divide; skipping the continuation produces an
empty exit. Tests also cover helpers after the entry, nonzero helper origins,
repeated local initialization, nested loops, Big/Real conditions and loop faults.
Ordinary Dis compilation is compared before/after scalar specialization.


### Series0060 validation and downloads

Source90318b08d6d4faae257b2f78dc0f9a6099097c46 passes
[CI41364908955](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41364908955):
native Linux ARM64 and x64, RV64/QEMU. Each archived helper execution gate
passes41 sources /82 whole interpreter/JIT processes plus2 active stale-local
controls and10 refusals. The independent FlashVM reader gate admits41 images;
its zero execution-process count is deliberately separate from runtime proof.
The previous case suite still passes67 sources /134 processes /67 stale-PC
controls per architecture. Downloaded execution logs confirm these counts.

| Target | Console, source/image/map examples and complete logs |
| --- | --- |
| ARM64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41364908955/artifacts/13106467395) |
| x64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41364908955/artifacts/13107301306) |
| RV64 QEMU CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41364908955/artifacts/13107255805) |

Local strict-UBSan compiler bootstrap repeats the41-source gate with82 whole
C/JIT processes. All27 previously accepted helper sources compile to identical
ordinary Dis and Dis64 bytes with the previous and new compilers. The owning
native-Limbo gate passes72 Dis/AOT/ABI32 cases. Readiness checks remain passing;
no native-fast-path or shared-file admission percentage changes in this series.

All three downloaded archives carry byte-identical procedures.dis64, SHA256
`d3248ef866dfebfdfbe8774abcacaa77fd39764f032340404291c05ea0d6b2bf`. Their execution environments remain as labelled above; this is
not a native RV64 producer, full shell or AWM acceptance claim.


## Series0061: private numeric tables in scalar helpers

Leaf procedures may now contain numeric case/range tables, dense goto dispatch
and Big case keys. The shared case validator checks table identity, bounds and
label records before specialization. Every arm/default must target the same
original helper, including its terminal RET. Each call copy owns independent
Case/Label/Type/initializer records, pointing into that copy's flattened body.
Two tables in one helper and nested cases remain separate. No source table or
entry PC mapping is rewritten. Existing case lowering then expands these private
tables into the unchanged43-op scalar profile and remaps final emitted PCs.

Per-helper table copies are counted before allocation, including every repeated
call. The terminal RET maps to that call's continuation. The16-function,
32-original-instruction,128-byte-frame and8-label bounds remain unchanged.
Arguments/results, recursive/nested calls and managed/string cases remain out
of scope. No new shared IDs or file ABI are introduced.

The owning gate now has60 accepted source cases. Local strict-UBSan compiler
runs pass120 whole C/JIT processes plus2 active stale-local controls and10
refusals. Real ordinary assembly must contain case/casel/goto for the new cases,
including multiple tables for the nested/two-table cases. Caller-only fault
sentinels prevent premature exit from passing. A fault in a helper case arm
must map back to the helper source line. All41 prior helper sources retain
byte-identical ordinary Dis and Dis64 output. The native Limbo gate still passes
72 Dis/AOT/ABI32 cases. Cross-target CI evidence is recorded separately.


### Series0061 validation and downloads

Source190a05e32bd7346fb6a9150b9e1872e7ce41dd4f passes
[CI41366374575](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41366374575):
native Linux ARM64 and x64, RV64/QEMU. Each downloaded execution log confirms
60 accepted sources /120 whole C/JIT processes plus2 active stale-local
controls and10 refusals. The independent reader log admits60 images and records
zero runtime processes; it is not substituted for execution evidence. The
previous case gate passes67 sources /134 processes /67 stale-PC controls on
all3 targets. No JIT coverage or shared-file admission percentages changed.

| Target | Console, sources/images/maps and full gate logs |
| --- | --- |
| ARM64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41366374575/artifacts/13106938337) |
| x64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41366374575/artifacts/13107671416) |
| RV64 QEMU CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41366374575/artifacts/13107039973) |

All3 downloaded archives contain byte-identical procedures.dis64, SHA256
`8924fde45642e7bbe0144e065fa9a243ae49bdf55848f83553ce070963e13a89`.
The actual downloaded x64 console independently retires loop/case/procedures
with JIT0 and JIT1. ARM64 execution evidence is native CI; RV64 producer is the
host compiler and its consumer runs under QEMU. No new physical Android, AWM,
managed file-call or native RV64 producer acceptance is claimed.

FlashVM can independently review/repeat the per-copy table target and terminal
RET continuation checks using this source checkpoint and the existing reader.
No reader extension or shared registry allocation is requested.


## Series0062: acyclic nested scalar calls

A scalar helper can call another scalar helper. The compiler validates adjacent
FRAME/CALL identities at every level, rejects direct/mutual recursion and calls
back to the exported entry, then computes finite expansion sizes before output
allocation. The whole flattened program and final scalar lowering retain the
65536-instruction bound. The16-function/32-original-helper-instruction/128-byte
helper-frame/8-case-label bounds remain. At most15 helper levels can be active.

Each active depth owns an appended scalar span. The largest helper at that
depth determines its size; sequential siblings reuse it. A child cannot alias
its parent's locals or temporaries. Declared locals are renewed per call, outside
that helper's loop backedges. Function/depth memoization bounds layout traversal
when shared helpers occur on several paths. Parent PC maps remain live while
child copies are emitted, so branches, case arms and early returns resume the
correct caller. Tables are private for each call copy, including descendants.

This remains source specialization into the unchanged43-op profile. No runtime
FRAME/CALL, recursion, arguments/results, managed objects or file calling ABI
are admitted. All60 prior accepted sources retain byte-identical ordinary Dis
and Dis64 output against0061. The bundled procedures example now calls bump()
from step() and fold() from a case arm in mix(), retaining caller Big/Real locals.

Local UBSan compiler tests pass74 accepted sources /148 whole C/JIT processes,
2 stale-local controls,2 active depth-overlap controls and12 controlled refusal
shapes. The overlap control rewrites the child's actual FP operands onto the
parent's local while preserving canonical encoding; the reader may still admit
this valid image, but execution must trigger the parent's zero-divide check.
Direct/mutual/entry cycles and exponential expansion are refused with a scalar
compiler diagnostic; a compiler signal or sanitizer failure cannot count as a
successful refusal. Tests cover maximum depth, shared descendants at different
depths, empty callees, conditional calls, parent/child early returns, nested case
calls, Big/Real preservation and leaf fault source identity. The ordinary native
Limbo72-case Dis/AOT/ABI32 gate also passes. Cross-target results follow below.


### Series0062 validation and downloads

Sourcefcfa0f9b6437f967a1d19da249f04825c4b8fb75 passes
[CI41369125880](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41369125880):
native Linux ARM64 and x64, RV64/QEMU. All3 downloaded execution logs confirm
74 sources /148 whole C/JIT processes,2 active stale-local controls,2 active
depth-overlap controls and12 controlled refusals. Reader-only logs admit74
source images and the valid semantic-control images with zero execution
processes; runtime controls are accounted separately. The earlier case gate
still passes67 sources /134 processes /67 stale-PC controls per architecture.

| Target | Console, sources/images/maps and complete logs |
| --- | --- |
| ARM64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41369125880/artifacts/13108642290) |
| x64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41369125880/artifacts/13108521604) |
| RV64 QEMU CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41369125880/artifacts/13108901497) |

All3 downloaded procedures.dis64 images are identical, SHA256
`7dc78de7be94da18b8999aec36a0a04b9181c5df0b7b38a5e1eca2cff98d4161`.
The downloaded x64 console also retires loop/case/procedures with JIT0 and JIT1.
ARM64 proof is native CI; RV64 consumer execution is QEMU, with a host producer.
The local UBSan compiler74-source gate,60-source old/new byte comparison,
ordinary72-case Limbo/AOT/ABI32 gate and readiness checks pass. Native fast-path
coverage and the43-op shared file profile do not change in this series.
No new APK, AWM, managed file-call or native RV64 producer claim follows.

FlashVM may independently replay this checkpoint with the existing reader and
review cycle/expansion refusal plus active parent/child overlap controls. No
format, feature-bit, coordinate-schema or registry extension is requested.


## Series0064: materialized scalar arguments

Acyclic helpers may take int/big/real values, without results. Source FRAME,
consecutive typed MOVW/MOVL/MOVF argument stores and CALL must agree on the
callee, descriptor, frame temporary, parameter order, offsets and widths. Source
values must already be scalar immediates or direct FP/MP values. Arithmetic,
indirection or side effects inside a pending argument setup remain refused;
constant-folded scalar values and values computed before FRAME are admissible.
Byte/managed/variadic arguments and scalar results remain outside this stage.

Each argument store becomes a direct copy into the child's private depth span.
Parent FP sources are relocated independently. The child's local initialization
skips all parameter cells, including both words of Big/Real values, while still
clearing padding and declared locals. Every call supplies every parameter before
entry; parameters are passed by value and child mutation cannot alias a parent.
The setup's interior argument stores and CALL cannot be branch/case targets.
Only its FRAME entry may be entered. The actual resolver is tested with compiler
records and an active removal-of-ownership-check control.

No runtime FRAME/CALL or file calling convention is introduced. Emitted code
uses the same43-op np0 profile; bounds remain16 functions,32 original helper
instructions,128-byte helper frames,8 case labels and65536 emitted instructions.
All74 previous accepted sources retain identical ordinary Dis and Dis64 bytes.
The example now passes scalar arguments through nested bump/fold calls, including
Big/Real values and a declared local renewed once per call.

The local UBSan compiler gate passes100 sources /200 whole interpreter/JIT
processes,2 stale-local controls,2 depth-overlap controls,2 argument-value controls
and14 controlled refusals. Changing a valid argument copy to zero must fail the
caller's semantic check; the independent reader may still admit that valid image.
Cases cover value mutation, reordered forwarding, locals/padding,12 Big arguments
at the frame boundary, conditional/case calls, large signed Word values and4097
calls. Cross-target CI and ordinary compiler validation are recorded separately.


### Series0064 validation and downloads

Source1060f2477957bf518a94c5106ed0e675d162ce2e passes
[CI41379799807](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41379799807):
native Linux ARM64/x64 and RV64 under QEMU. Each downloaded archive confirms
100 sources /200 whole C/JIT processes,2 stale-local controls,2 depth-overlap
controls,2 argument-value controls and14 controlled refusals. The actual setup
resolver and its active ownership-removal control pass on all3 runners. The
external FlashVM reader admits100 source images; its zero runtime processes
are reader evidence, separate from the actual VM execution above. Existing
case gates retain67 sources /134 processes /67 active stale-PC controls.

| Target | Console, source/images/maps and full gate logs |
| --- | --- |
| ARM64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41379799807/artifacts/13112611825) |
| x64 native CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41379799807/artifacts/13112419937) |
| RV64 QEMU CI | [Download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41379799807/artifacts/13113121055) |

All3 downloaded executables are ELF64 for their expected machine, and their
procedures.dis64 bytes match SHA256
`dd7e4f02c712f8e0a0d6fdf3d641518b241eb4c828b0321a9cb44f906128be1e`.
The downloaded x64 console independently retires loop/case/procedures with
JIT0 and JIT1, empty module exit text and an empty scheduler. Existing Linux
cleanexit terminates the host process group; OS status is not used as the
successful module-execution criterion. Native ARM64 execution proof is CI;
RV64 uses a host compiler and QEMU consumer. No new phone, APK or full AWM
acceptance is claimed. Local strict-UBSan bootstrap, ordinary72-source
Dis/AOT/ABI32 gate and readiness checks also pass. The previous74 helper sources
retain byte-identical ordinary Dis and Dis64 against the earlier compiler.

Fast paths remain166/173 for ARM64/x64; RV64I87, Zmmul90, M103. JIT plus C
remains173/173. Shared-file admission remains43/173, not full application
readiness. Scalar results, general file calls, managed storage/imports and full
shell startup remain separate work. FlashVM may independently repeat this
checkpoint using its unchanged reader; no shared format allocation is requested.


## Series0065: scalar expressions in argument setup

A helper argument may now be produced directly by Word/Big addition, subtraction,
multiplication, division, remainder, bitwise AND/OR/XOR or signed shifts. The
producer must write precisely one declared parameter in order. Arithmetic needs
an explicit middle operand: an omitted operand would read an uninitialized child
cell. Both input operands are checked against the caller geometry and cannot
alias its opaque FRAME temporary. Caller FP sources and middle operands relocate
independently before the destination becomes a direct child-depth cell.

MOVW/MOVL/MOVF retain their existing type and no-middle requirements. No new
opcode is allocated; source operations lower into the agreed43-op scalar profile.
Floating arithmetic, result pointers/LEA, managed arguments, indirection, nested
calls during an outstanding setup and arbitrary setup side effects remain closed.
The earlier setup-interior branch/case refusal still applies to computed stores.
Expression faults occur before the callee body, with their original source PCs.

Local bootstrap and whole x64 VM gates pass126 sources /252 C/JIT processes,
all6 active VM controls and14 refusals. These include20 Word/Big operator cases,
zero-divide and invalid-shift argument faults, nested FP input/middle operands,
multiple arguments and live caller locals. The actual setup resolver/removal
control passes. Ordinary scalar compiler tests pass25 source/refusal cases and
30 whole VM processes. Cross-target CI must be checked separately; these local
results do not establish native ARM64/RV64 execution of this new checkpoint.
