# INFERNO-0180 — compiler .m requests and native namespace admission

2026-11-14. Code base8732eedd; preserved peer journal92307ccf/FLASHVM-0197.
Retained native debug continuations and whole-emitted-function JIT quarantine are
scoped peer reports; native signal crash capture/recovery remains explicitly
absent there. Reported12/12 ordinary/TSan selections, list62, completed previous CI
and not-yet-completed new matrix are not Inferno architecture evidence. No peer
implementation/concurrency transfer, public ID or vendor pin change is introduced.

## Compiler interface producer

Optional limbo -H -R -M Module emits the existing scalar Imports/Strings payloads
as const byte arrays and per-function LIMBO_NATIVE64_IMPORT request ordinals.
Requests come from the actual typechecked .m interface, including unused functions;
provider exports/code/maps never supply client metadata. The ordinary native64
C declarations, frame/layout predicates and compiler-generated classic hash
constants keep their meanings. Default -H bytes remain unchanged.

Before any C output, the emitter checks every function/result/argument, bounded
ASCII names, function/parameter counts and reserved C identifiers. Supported
request kinds stay void/int/big/real. Empty requests,64 functions/parameters and
255-byte identifiers pass. Byte/fixed, managed/by-value aggregates and variadic
requests refuse; the wider existing ordinary -H support is preserved. Both payload
arrays are serialized through existing signature/string/binding codecs. This is
no new module file or public request format/version.

Five positive producer boundaries pass with independently parsed exact kinds,
names, counts and macro ordinals. Twelve well-typed interfaces accepted by ordinary
-H refuse -R without C output. Two reserved-prefix declarations retain their
ordinary/opt-in refusal, and three invalid CLI combinations refuse. Ordinary -H
matches the prior compiler on all five boundaries and14 real provider interfaces.
The host compiler bootstrap is built with ASan/UBSan.

## Native namespace reader

New read64calllibrary accepts a path, JIT request, raw Imports/Strings payloads and
sync0/1. It checks/seals the existing explicit dis64-calls startup policy and
preflights request metadata/path before namespace use. Default/disabled modes,
$ builtin/self paths, invalid flags and malformed requests refuse without I/O.
No filename or wire bit chooses a host execution policy.

The protected read owns FD/Dir/image, bounds signed file lengths before native
allocation and joins short reads while checking returned counts against the
remaining extent. It closes the FD before admission. Native kclose consumes the
owned descriptor on every returned status. sync1 releases/isaves only for the read
and acquires/irestores before admission/construction; sync0 holds exclusion.
Existing caller-lease release guards apply, and caller buffers must stay live/
immutable through I/O. The current global dontcompile policy is evaluated after
I/O, including a policy change while VM exclusion was released.

Under exclusion, the existing mustbesigned predicate precedes even the constructor
proof workspace. Then0179's library constructor rechecks exact module/symbol/full
signature and proves all provider bodies before creating owned Types/code/JIT and
one rooted instance. Partial native resources, denied/throwing admission, parser/
constructor failures and final pin failure retire completely. Success returns one
reference and one external pin; normal mapped-root/barrier transfer and ptrdel
remain the caller's responsibility. Entry/entryt stay nil: loading invokes no init.

Every call opens/reads/admits/proves and creates a fresh unregistered instance.
The bridge does not use lookmod/readmod cache shortcuts or legacy hash lookup.
An existing same-path module cannot supply its admission. It attaches no reusable
namespace/cache provenance capability. The guest ILOAD handler is unchanged;
there is no new admitted serialized Limbo importing body or managed graph.

## Runtime evidence

Fourteen actual source interfaces and scalar providers run through compiler -H-R
metadata, the production reader/admission route, whole-image constructor and
MFRAME/MCALL/CALL/RET. Each x64 and ARM64/Linux QEMU gate executes672 scheduler
roots:14 x four client/provider C/RX choices x fitting/growing stacks x quanta
1/3/1000 x two I/O modes. Actual stack growth, exact scalar result bits, accepted
RX on normal families, input poisoning, no implicit init and explicit error
retirement pass. Fault-only leaves can use C fallback without an emitted RX entry.
The outer importer is trusted native fixture code using compiler-generated
request ordinals; the complete provider comes from its compiled file.

Every read/constructor allocation failure, final root failure, nine I/O failure
forms, invalid/negative/oversized/full-width file lengths, malformed file/request
bytes, denied/throwing admission, stale module aliases and host JIT policy pass.
The fixture verifies that admission and construction occur only after acquire and
FD closure. Independent .m variants request an unused missing export or change
Big to equal-width Real: both refuse before VM execution across JIT/I/O modes.
Default mode and a feature-disabled target binary perform no namespace/constructor
work. Host materialize-only ASan/UBSan and x64/ARM64 runtime UBSan pass. The known
runtime-ASan unwind limitation remains outside VM instruction execution evidence.

Four active omissions remove admission, FD cleanup, acquire ordering or startup
policy. Both targets require precise semantic exit10, complete native/FD/root/
scheduler cleanup and no malformed execution. The test admission boundary repairs
an omitted acquire before raising its diagnostic so the control remains safe;
this is not production fallback. Reader/scheduler/ownership bodies are production;
namespace transport, admission predicate and GC/pin/OS seams are bounded fixture
support. No actual filesystem-concurrency, collector or new full-OS guest LOAD
execution is inferred.

## Build and compatibility

Full classic-mk x64 and ARM64 consoles are rebuilt with current readmod/interp
sources. The full build exposed the missing extern declaration for the existing
dontcompile host policy; it is now in the shared runtime header. This changes no
Module/Frame layout. Each full console retains56 executable-file OS roots and28
library standalone refusals. These are compatibility runs, not positive new
namespace reader/guest LOAD boot evidence.

Prior14-library/336-root buffer-constructor gate remains green on host. Compiler
.m tutorials, C embedding contract and RU/EN reproduction docs are added. New
producer, namespace, disabled and four-control gates enter native64-port.py.

## Next client boundary

Compiler-generated client interface bytes and native file admission now connect.
Guest importing Limbo CODE, owned structural import tables/module-reference roles,
namespace/cache/ABI admission for its LOAD instruction, general managed values and
remaining heap operations remain ahead. Android/Bionic GUI, AWM and a working APK
are unverified; NDK/device execution is unavailable in this environment. Shared
managed signature/type expansion still needs coordination. Existing builtins,
classic hashes, scalar policies and vendor pin keep their contracts.

Peer0197's function-boundary seam is compatible with the complete export catalog:
Inferno already derives exact entry/end ranges for every emitted admitted scalar
function. The producer disables inlining. Those proof records are disposable
workspace today; persistent debugging/quarantine would require independent owned
retention, not borrowed pointers or a restored REG snapshot. No such feature is
implemented or imported in this namespace batch.
