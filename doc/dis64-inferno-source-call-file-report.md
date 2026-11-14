# INFERNO-0177 — Limbo source call files and complete console boot

Follow-up: [INFERNO-0178 nested call DAGs](dis64-inferno-nested-call-file-report.md).

2026-11-14. Code base860a9d2b; preserved peer journal4f61361c.
FLASHVM-0194 read/retained, including receipt of0176. Copied owner name/path/next
original Dis PC, native label limits/TLS-managed claim and frozen ticket reader
semantics remain scoped peer evidence, not arbitrary-pointer or concurrent module
lifetime authority. Selected10/10 ordinary/TSan, earlier CI completion and new
pending architecture execution are recorded as peer reports. No peer implementation,
private protocol, VM concurrency model or vendor-pin change is imported.

## File path implemented

New limbo -64c emits complete scalar caller/leaf files for the existing explicit
-X dis64-calls -d path. It disables source inlining, retains all compiled exported
functions, orders init first and relocates functions/calls/branches. Existing
-64/-64m/-64l/classic paths retain their meanings. Root is argument-free void init;
root is linear, helpers are scalar leaf functions with branches/loops/returns.
Module data/imports/handlers, managed references, byte signatures, nested helper
calls, root branches and compiled nonexported helpers remain outside this producer.
Limits256 translated instructions,64 functions/types/parameters,1MiB frames/image.

Typed multi-call lowering supplies complete source/native scalar coordinates.
Every frame receives expression scratch and canonical outgoing result-tail cells.
CALL results are copied into source locals after normal return; arithmetic return
expressions compute into scratch followed by an exact typed FP32-result-address
store. Relocations account for all inserted instructions; root RET becomes EXIT.
The whole encoded image then passes dis64callinfercheck before the first Bwrite.
Actual raw numeric locals/initialization/call/result/return proof is recomputed,
without accepting compiler-local declarations as file authority.

Files use existing Dis64v1.0 ABI1 Module/Code/Types/Exports/Strings and scalar
signaturev1/opcode framing. -64c is a producer selector, not a new public wire
version/profile/extension/section/tag/ISA ID. Existing named launch/cache policy
and ordinary admission callbacks remain; the default launch refuses these files.
This uses the established raw-call constructor/reader, with the trusted owned
client/import constructor retaining its separate contract.

## Word32 to Big

Raw inference and root call proof now explicitly support existing ICVTWL alongside
ICVTWF. Word source kind/definite initialization and Big/Real destination kind are
checked; no numeric seed or declaration is supplied externally. Five new bound
cases cover signed immediates, Word source, wrong source/destination and missing
initialization. Existing conversion source/destination active controls remain
precise. This does not open every conversion opcode in arbitrary helper bodies.

## Execution and refusal evidence

Sixteen complete Limbo programs produce deterministic bytes with/without explicit
-i. Python independently parses all emitted sections/instructions/signatures and
checks against classic source reports and the independent caller/leaf oracle.
All sixteen classic outputs match the previous compiler with identical paths/flags.
Cases include repeated calls, two args, branch/loop/return relocation, init emitted
after helpers, int/big/real/void, negative values, Word32 min/max sign extension,
three-function catalogs and root/leaf zero divide.

Each x64 and ARM64/Linux QEMU file gate executes sixteen actual files in C and RX
through readmod/parsemod/cache/admission/root scheduler and full retirement. It
fails every read/parse/construct allocation position and checks zero live owners,
pins and balanced file descriptors. It checks exact result bits independently,
including sign extension at both Word32 boundaries. Eight damaged images refuse
and ten unsupported sources publish no image payload. Default/cross-profile/cache
refusals and input destruction after lowering remain checked.

Both complete console emulators were rebuilt with matching current runtime/header
sources. Each runs32 actual OS roots compiled from these same source cases:
28 successful completions and four zero-divide/error retirements, C/RX. Full boot
checks real startup/vmachine/worker/retirement and default rejection; numerical
value checks belong to the separate file gate. These are Linux x64/ARM64 QEMU,
not Android/Bionic/GUI evidence. Complete console uses normal target flags;
bounded runtime uses UBSan. Host compiler and writer IR unit use ASan/UBSan.
Existing runtime-ASan stack-unwind limitation and bounded fixture GC/OS seams
remain open/scoped.

The allocation-accounted writer IR unit checks complete publication, corrupted
uninitialized raw IR refusal, no output payload on semantic failure and allocation
retirement. Three active omissions remove final raw proof, canonical result binding
or relocation-array cleanup; exact semantic assertions are required. The unit and
all controls pass host ASan/UBSan and ARM64/QEMU UBSan.

## Compatibility and build

Prior raw inference9 images/30 atomic refusals/312 C/RX runs and all four controls
pass;3343 catalog-bound cases plus conversion source/destination controls pass.
Host previous owned-import client/provider960 and its controls pass. All16 AWM
outputs/376 layouts retain classic bytes against the previous compiler; no AWM
execution is inferred. Classic mk now lists the new Limbo object and proof-header
dependencies, so incremental VM builds recompile changed verifier headers. Port
driver includes writer, source-file runtime and complete source-file boot gates.

RU/EN commands/examples: dis64-source-call-files.ru.md / .md. The supported
source→file→readmod→console path is now executable. General managed client/import
serialization, dependency load/namespace/cache ABI admission, remaining heap
operations and Android GUI remain ahead. No general managed signature/tag IDs
are selected by this step; coordination remains necessary before expanding shared
managed wire contracts. No ready APK is claimed.
