# INFERNO-0179 — scalar provider files and structural library instances

2026-11-14. Code basef01cdf6f; preserved peer journal0e7ab3df.
FLASHVM-0196 read/retained: coherent allocator counters, exact live gauges versus
saturated history,96-byte scalar snapshot publication, reader ownership and CI
queue/cancellation scope. Reported14/14 ordinary/TSan selections and architecture
list60 remain peer evidence; pending CI is not full matrix completion here.
No peer implementation, vendor pin or VM-concurrency model is imported.

## Producer and proof

New limbo -64b selects the EXISTING library metadata profile0, including modules
with a function named init. Existing -64c also emits profile0 when no init entry is
present; with init it retains its existing executable policy. Both disable source
inlining. Classic/-64/-64m/-64l output meanings remain unchanged. No public wire
ID/version/section/feature/opcode/signature kind is allocated.

The same scalar writer now emits complete libraries in compiled function order,
without a synthetic command root. Library Module metadata is name/profile/data-
size only: no entry fields or sentinel. Every compiled function still needs a
matching declared export. Ordinal0 can take arguments, return a result, call
another helper and be a local target. All functions use RET; EXIT is refused.
Local DAG callers, outgoing result-tail ownership and inserted return copies
reuse0178. Call-free leaves retain their CFG/loop/return proof.

Raw dis64calllibraryinfercheck derives canonical scalar spans, parameter seeds,
actual CALL dependencies and full function facts. Nonvoid return coverage applies
to function0 too. Self/mutual cycles, malformed owners, missing initialization/
return stores and resource exhaustion refuse before records/count/image publish.
Metadata/code scratch may change on refusal. Old executable wrappers retain their
profile/root policy and exact accepted proof summaries.

Limits remain256 translated instructions,64 functions/types/parameters,1MiB frames
and producer output. Data/own imports, managed references, byte signatures,
handlers, recursive calls, caller branches and nonexported compiled helpers are
still outside the producer. The raw runtime accepts only the existing five
Dis64v1.0 ABI1 sections and scalar signaturev1. No generic managed graph is written.

## Structural requests and owning construction

New prepare64calllibrarylink accepts the immutable complete provider image and
existing raw Imports/Strings payloads, not C PC/map/function declarations. Exact
module name, symbol and full scalar signature must match. Equal-width Big/Real
are distinct. Request ordering determines link ordering; duplicate exact requests
retain existing codec semantics. Zero and64 requests instantiate;65 refuses before
allocation. Matching outputs stay unchanged on late failures.

After matching, the runtime proves EVERY provider function, even with zero
requests. It owns copied Types/maps/code/labels/JIT and constructs a production
mklinkmod instance with proved export frame/PC coordinates. It leaves Module.entry
and entryt nil. No function, including init, is invoked. A classic32-bit hash is
neither guessed nor installed in Module.ext; positive request links carry the
structurally matched coordinates. Existing classic hashes and the trusted owned-
import constructor keep their separate contracts.

Construction runs under VM exclusion with unmonitored allocation. Success returns
one instance reference plus one external GC pin. Caller must transfer to a mapped
VM root using ordinary reference/barrier rules, then ptrdel. Failure retires all
partial module/type/code/JIT/link ownership, including a final pin-acquisition
failure. There is no fallible work after successful pinning. Pure libraries have
no MP data. Input image/request/name buffers may be destroyed before execution.

This API does not read a namespace, cache a module or admit a guest LOAD; path is
only an owned label. The importing caller remains trusted native code. This is a
provider-file/structural-instance milestone, not general source-importer admission,
legacy/native ABI mixing or authentication of arbitrary C manifests.

## Execution and controls

Fourteen complete libraries have independently checked wire/code/signatures,
source-layout comparison and Python caller/leaf/DFS proof. Leaves are isolated
behind a test-only EXIT root for the older independent CFG/return oracle; that
root never appears in the emitted library. Both -64b/-64b -i bytes match; modules
without init additionally match -64c bytes. All14 classic outputs match the prior
compiler using identical paths/options. Ten unsupported whole sources emit no
payload, including byte/String/array/list/tuple, globals, module arguments,
recursion, caller branches and private compiled helpers.

Each x64 and ARM64/Linux QEMU runtime gate executes336 actual scheduler roots:
14 libraries x four client/provider C/RX combinations x fitting/growing stacks x
quanta1/3/1000. Cases include empty/void, a single export, local targets at entry0,
first-function callers, several depths, branch/loop leaves, int/Big/Real, mixed
aligned arguments and two fault families. Twelve normal families require genuine
accepted RX dispatch; fault-only constant-zero division can have no emitted RX
entry and use C directly. Requested JIT mode never means every instruction is
native. Production MFRAME/MCALL/CALL/RET, live REG on terminal error and full
scheduler retirement are used.

Exact result bits, all requested links, copied-input poisoning, every actual
constructor allocation failure, zero/64/65 request bounds and final pin failure
pass. A library containing a dividing init is constructed without executing it;
only an explicit imported init call faults. Complete proof also refuses a corrupted
body with an EMPTY request table. Numeric/map/provider code comes from the file;
the outer importer is trusted fixture code, not a newly admitted Limbo client.
GC/pin and OS seams remain bounded fixture support, not a new collector/concurrency
proof. Host constructor-only ASan/UBSan and x64/ARM64 runtime UBSan pass; the existing
runtime-ASan unwind limitation remains open.

The separate raw library gate passes74 positive/negative/resource cases with
unchanged refusal records/count/image on host ASan/UBSan and ARM64/QEMU UBSan.
Three active omissions remove ordinal0 return coverage or self/mutual dependency
protection. Four matcher/constructor controls remove nominal module checks,
complete signature equality, no-entry construction or atomic match publication.
Precise semantic exit10 is required, and malformed modules are never executed.
The writer IR unit now covers library publication too; all three proof/binding/
cleanup controls pass host ASan/UBSan and ARM64/QEMU UBSan.

## Compatibility and remaining work

Both full classic-mk consoles are rebuilt from current runtime/header sources.
Each retains56 executable source-file OS roots and refuses28 standalone library
selections across C/RX. These are executable compatibility/library-refusal runs,
not new positive full-console library LOAD evidence.

Previous28 source files,18 damaged files and ten whole-source refusals remain green.
Old94 DAG cases/six controls retain exact executable proof summaries. Prior3343
bound cases retain389 accepts/2954 refusals. The actual old constructor corpus
retains20 accepted/60 refused constructions,170 allocation refusals and all four
ownership/lowering controls. Classic mk includes the new header dependency;
native64-port.py includes library runtime, raw proof and four constructor controls.
RU/EN contracts/examples: doc/dis64-scalar-libraries.md / .ru.md.

General managed provider/importer serialization, serialized module references,
namespace/cache/ABI admission for guest LOAD, remaining heap operations and Android
GUI remain ahead. No Android/Bionic/AWM execution or working APK is claimed; no
NDK/device execution is available here. This batch deliberately keeps the shared
managed signature/type-ID expansion and FlashVM implementation transfer separate.
