# Private compiler scalar-field witnesses

This is an opt-in trusted C contract, not a Dis64 section, capability, opcode or
calling convention. It follows FLASHVM-0049. The shared43-op np0 file profile,
execution_supported=false and file CALL/MCALL admission are unchanged.

## Compiler descriptions

Limbo -H with LIMBO_NATIVE64_FIELDS selected emits LimboNative64Fields alongside
the existing exact LimboNative64Entry. The six-field entry layout is unchanged.
The table contains every top-level source argument's ordinal, kind, offset and
complete declared width. Ordinals include references and aggregates; padding,
Frame64 links, the return pointer and temporaries are not declared arguments.

The first profile marks only Big and Real as eligible, using the already agreed
canonical Dis64Big=2 and Dis64Real=3. Zero marks an ineligible field here; it does
not assign a reference kind or admit a Void parameter to canonical signatures.
Byte/int/fix, references, nested tuple members and other fields are described but
not eligible scalar destinations. No wire kind or feature ID is invented.

An empty function has a zero-count description with ISO C storage. Variadic
prefixes have no callable field table. More than4096 fixed arguments are refused
in compiler preflight before output. Added C identifiers participate in the
existing collision registry. Legacy declaration generation and file bytes stay
unchanged. LIMBO_NATIVE64_FIELDS implies the existing ENTRIES selection; tables
remain trusted immutable compiler-owned C data, not authenticated arbitrary input.

## Runtime binding

Native64FieldWitness in include/native64-field.h binds one declared eight-byte
field to an exact current native caller Module, Modlink instance, Type and
exported C callable. It verifies complete generated frame size/map, unique source
symbol, implementation address, descriptor-table membership and the legacy hash
as a consistency check. A same-hash different implementation is refused. The
selected field kind is obtained from the compiler record, never inferred from
an all-zero GC bitmap, a width match or a caller-supplied byte offset.

Acquisition is allocation-free. It first requires current Pready scheduler
identity and the selected saved/live register contract. Noncurrent/blocked callers
are rejected before reading their module metadata, including deliberately poisoned
metadata in the fixture. Published/live divergence is refused before following
saved module roots. Then the existing noncopyable caller lease pins Type and
holds scheduler ownership; the witness acquires one Module reference.

The retained Module/Type prevent their ordinary retirement and pointer reuse;
immutable generated/export tables provide the callable identity, and the existing
saturating caller epoch supplies the activation generation. No new Module refcount
scheme or serialized generation is introduced. The caller's rooted current
Modlink and trusted immutable registry/storage remain explicit obligations. The runtime does not authenticate an arbitrary C
table as compiler output; the trusted native constructor/call site must pair the
field table with its original generated callable record. This is not file
metadata provenance or a replacement for structural signature admission.

Late resolution requires the same instance, MP, SP/TS, export and descriptor
arrays/count, exact generated entry and field-table identities, selected source
ordinal/kind/offset/width and exported callable. It also repeats current caller,
epoch, cancellation, frame-boundary and map checks. Paired mutations of both live
and saved register roots cannot silently retarget this witness. Checked rebind
uses the existing within-extent transfer and updates live/saved FP together.
Clear detaches its outer identity, releases the acquired lease and drops exactly
one Module owner; copies and repeated clear never retire the original owner.

## Protected owning producer

Math_bits64realfield64 accepts a canonical import/input request, current caller,
exact Module/Type, compiler field table and source ordinal. It requires a declared
Real field and acquires the full witness before any producer allocation. The
existing protected Math_bits64real64 produces into its private scalar result;
late witness resolution precedes the callback-free eight-byte commit. Witness
retirement is outside the protected producer frame and preserves its diagnostic.
The disabled entry refuses before reading its request. Other existing private
adapters retain their previous trusted contracts.

This is not generic function-local metadata or managed argument/result transport.
The described caller fields come from fixed C/native frame declarations. It does
not admit arbitrary bytecode caller frames or file-native imports. Independent
detached Prog lifetime, moving/suspended extents, asynchronous waits and general
managed call graphs remain open. Earlier optimized UTF/unwind ASan findings remain
open; these tests do not close them.

## Evidence and reproduction

```
python3 utils/tests/native64-field.py --limbo /host/tools/limbo
python3 utils/tests/native64-fixed-math.py --caller-guard --inject-callee \
  --limbo /host/tools/limbo --libdir /build/Linux/arm64/lib
```

The field gate executes actual compiler output, the production witness/caller
headers and owning Math_bits64real C production. Its351 predicates per WORD32
model cover Big/Real identity, padding and aggregate/reference refusal, an aligned byte-plus-padding cell accepted by geometry alone, exact
Module/Type/callable binding, copies, checked transfer, stale generations,
changed metadata, paired root mutations and refusal before poisoned inactive
metadata. Empty/variadic and4097-argument compiler controls are independent gates.
Four compiled kind/owner/generation/field removals reach active semantic failures.

Scheduler identity, Type retirement and Module retirement are bounded in this
standalone fixture. It is not a collector or independent last-owner lifetime
proof. The owning protected Math gate separately uses the existing constructor,
Type/Module and kprotect paths, preserving68 exports,141 allocation refusals and
12672 scalar cases per model. Typed destination checks add both saved/live REG,
pre-allocation Module retention, cancellation, nested refusal and real errors
before allocation or after the actual callee scalar store, with unchanged failed
results and balanced references. Existing actual GC gates remain separate evidence;
this series does not claim a new managed-field witness/collector integration.

Native x64 strict O3/NDEBUG UBSan and ARM64/QEMU field/producer checks pass. The
standalone field gate and its four controls also execute under RV64/QEMU using
the GNU Linux RV64GC C toolchain; this is not an RV64I JIT or full-emulator boot
claim. The reverse-contributed sequence driver now accepts NATIVE64_RUNNER and
NATIVE64_TMPDIR. It executes131072 traces plus four semantic controls on x64,
ARM64 and RV64, with the original bounded scheduler/descriptor services.

The extended clean classic-mk port driver includes field and control stages.
ARM64 and x64 complete scalar emulators retain pilot JIT0/1 boot and closed
legacy/builtin imports. ARM64's744-image/1667800-predicate runtime, scheduler,
file-loader and vmachine gates remain passing. Native fast paths and full JIT+C
opcode counts are unchanged.
