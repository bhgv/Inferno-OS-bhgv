# INFERNO-0175 — source-derived imported calls and native execution

2026-11-14. Base19d2962d. Follow-up to INFERNO-0174. FLASHVM-0190/0191/0192
remain acknowledged; no newer peer journal entry was present at the prepublication
fetch. Their selected-test/transport limitations remain as recorded. No peer
VM, concurrent ownership model or private protocol is imported.

## Compiler boundary advanced

The opt-in LIMBO_DIS64_TYPED_EXTERNAL report lowers complete fixed-arity imported
call bodies. Final compiler IR already supplies function declarations, existing
sign(Decl*) hashes, qualified names and iface->offset import ordinals. Canonical
MFRAME/ordered argument moves/result binding/MCALL islands require the same module
operand and exact import declaration at both ends. Module operands must be typed
FP/MP module roots; ordinals are0..32767. Scalar/reference native alignment and
source coordinates are checked. Aggregate copies, varargs, polymorphic functions
and exception handlers still refuse. Branches can reenter MFRAME but never its
interior. Inherited MCALL establishes the result only on normal completion.

The per-root multi-body cache and CFG/result analysis are shared with local calls.
New owned external-site storage is retired on success/refusal. Existing profiles
are unchanged when the flag is absent. inferno-typed-external-v1 is private checked
compiler metadata, not a public bytecode format, signature scheme or ISA ID.
Ordinary Dis output is identical with and without reporting.

## Native materializer and runtime

Private Native64TypedCall adds external/module-space/module-offset fields.
Local calls keep these zero. External sites carry an import ordinal and linked
Type bounds, without an embedded external native target PC. Atomic validation
checks both endpoints, frame/argument/result spans and branch isolation before
writing any output. Actual MFRAME/MCALL performs linked target lookup at runtime.

The source oracle independently reads classic instructions and real serialized
.m import tables:17 complete client functions,33 imported sites, local/inherited
results, void functions, managed nil and source loop reentry. A separate mixed
byte/int/big/string case checks native argument offsets64/68/72/80. Four altered
reports are refused. A loader helper creates real import hashes but its typed
body remains refused. Both managed file profiles(-64m/-64l) reject the whole client.

Each x64 and ARM64/Linux QEMU run passes912 runtime cases:768 ordinary C/RX pairs,
48 source-loop cases and96 injected managed continuation faults. Quanta1/3/1000,
stack growth, two inputs, independent provider/client JIT choices, source nested
provider calls, scalar widths, nil reference values, fresh/inherited result
addresses, restored caller Module/MP, separate active/idle MP clones and complete
stack/module/managed allocation/pin teardown are checked. String/list values are
opaque owned graphs in this fixture; no extra actual String/list operation proof
is inferred. Imported-module MP operands have materializer evidence; runtime
source clients use FP module arguments. Mixed-argument geometry is source-oracle
evidence, not an additional runtime matrix.

The provider uses the owned whole-module constructor from0174. Client descriptors,
module ownership and the outer invocation harness are assembled by trusted C;
complete inner client bodies come from Limbo. Automatic import-table construction
inside Native64ModuleSpec, namespace/cache admission and general image loading
are not implemented by this step. Linked native Type/semantic-role/provenance
premises remain trusted; this bridge does not validate arbitrary file pointers.

## Negative controls and sanitizers

Eight materializer refusals preserve the entire output buffer, with successful
FP and MP canonical bindings. Three active controls remove module alignment,
import-ordinal bounds or setup-branch isolation. A fourth removes production
linkm hash equality. Four compiler controls remove matching module identity,
argument coordinates, ordinal bounds or inherited-result tracking. Only exact
semantic diagnostics count; no crash/timeout/sanitizer result is accepted.

Compiler IR passes host ASan/UBSan and ARM64/QEMU UBSan, including all four active
controls and allocation retirement. The ASan/UBSan-built host compiler produces
all oracle fixtures. Host constructor/materializer-only ASan/UBSan and all three
materializer controls pass. Runtime x64/ARM64 uses UBSan; existing runtime-ASan
stack unwind limitation remains open. Pin/OS/collector seams remain bounded
fixture support, not a concurrent collector or resident scheduler proof.

## Regression and next boundary

Host prior module960/four controls, multiple-local-call480/four controls/1728
binding refusals, compiler multi-IR/four controls and default typed-body/five
controls pass. All16 AWM outputs/376 plans retain classic bytes against the
previous compiler. Default AWM remains14 accepted/346 refused, not an AWM run.

Port driver now includes external-source execution and compiler IR gates. RU/EN
examples and contracts: dis64-typed-external-calls.ru.md / .md. Next work is whole
client import ownership/assembly and general managed writer/loader/load admission,
then remaining heap operations and GUI startup. Existing scalar/Sys/file profiles
keep their separate permissions. No new public IDs, Module/Frame ABI changes,
Android/Bionic device execution or ready APK is claimed.
