# INFERNO-0181 — owned scalar request tables and scheduled ILOAD

Base: experiment bd4e9411c4931c5ef0a970ebbfc9d409b8750973 (INFERNO-0180).

Production ILOAD now delegates to the scalar namespace library reader when its
caller has explicitly installed owned structural requests. The new trusted API
install64callimports copies complete independently compiled `.m` Imports/Strings
for up to64 tables into one bounded owned allocation. It rejects classic LDT
mixing, duplicate installation, shared/cached callers, unsupported native callers,
invalid startup/JIT selection and all malformed tables before publication.
Failure leaves both private Module fields empty; freemod retires the owner.

The callback belongs to trusted native code; no native function pointer or owner
is decoded from a file. Two private Module fields change native Module layout and
require a complete runtime rebuild. Frame64/Modlink, signaturev1, public Dis64
section/ISA IDs and vendor pins remain unchanged. Ordinary -H output and the0180
interface producer remain unchanged.

ILOAD's middle operand selects a structural table, independently of MCALL's
function ordinal. Negative/out-of-range tables raise exModule before namespace
I/O. The reader preserves explicit startup selection, fresh admission, exact
structural matching, whole-library proof and current dontcompile policy. It
releases/isaves only during protected native reading and acquires/irestores before
admission/VM construction. The importing Module's VM context retains the copied
requests across this interval.

On success ILOAD marks/publishes the new mapped destination before removing its
external pin and destroying the prior instance. Normal namespace/interface refusal
publishes H and drops the old destination owner. Invalid indices raise without
changing that destination. No implicit init; no builtin/self structural library
admission; classic/trusted Sys paths remain selected for callers without the new
callback. Existing namespace error and constructor cleanup contracts remain intact.

## Execution evidence

The extended dis64-library-namespace.py --iload gate uses the actual production
ILOAD handler, reader, provider constructor, scheduler, MFRAME/MCALL, return and
reference destruction. Every provider/interface still comes from actual Limbo
source and `.m` compilation. The importer code/root maps remain trusted C metadata.
ASCII filename conversion, namespace transport/admission and pool/GC support are
bounded fixtures. This is no untrusted importing-client image admission or new
concurrent filesystem/collector proof.

Each root executes two loads through different table indices: empty requests,
then the complete `.m` interface. Per-instance nlinks confirms the distinction.
The second load replaces the first owned instance. Requests and names are poisoned
before execution; no emitter buffers/stack descriptors remain borrowed. Quanta1
checks the absence of temporary pins between load instructions. Genuine accepted
RX leaf entries are observed for normal scalar execution.

Four caller/provider C/RX choices × two stack geometries × three quanta × five
outcomes give120 roots/provider. Fourteen providers give1680 roots on x64 and
ARM64/Linux QEMU. The five outcomes cover ordinary completion/provider faults,
open refusal, denied admission, upper-bound table index and negative table index.
Two independent `.m` mismatches add240 refusal roots per target: unused missing
export and Big/Real mismatch. No mismatched provider is called. Installation also
checks64 tables, zero function requests, metadata/allocation failure, cached/shared
callers and malformed late tables.

Three active controls remove request copying, pin removal or structural dispatch.
They must stop with semantic exit10 without sanitizer/UB failure. They pass on
x64 and ARM64. Host ASan/UBSan covers complete installation and retirement without
VM execution. The known runtime-ASan instruction/unwind limitation remains open.
The disabled/default gate also refuses installing a caller table without allocation.

Both complete classic-mk consoles are rebuilt after retiring all old C objects and
archives. Each retains56 real executable OS roots plus28 standalone library
refusals. The existing Sys registration gate retains two-export production
ILOAD/linkmod and seven rollback cases; its broader registration/profile checks
also pass. These full-console results are compatibility checks, not a new positive
full-OS structural importing-client test.

Gates enter native64-port.py. Tutorials:
[EN](dis64-scalar-iload.md), [RU](dis64-scalar-iload.ru.md).

## Remaining boundary

The importing client's CODE, module-root roles and structural table grouping still
need compiler production plus complete untrusted-file proof and owned materialization.
Installing a callback for a native-authored test caller does not provide that proof.
General managed file graphs, native C builtin import coverage, Android/Bionic/AWM
and a working GUI APK remain separate unfinished work. This batch imports no
FlashVM code/debugger/quarantine and changes no shared managed IDs or vendor pin.
