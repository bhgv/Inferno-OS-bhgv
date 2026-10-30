# Experimental native64 declarations from the real Limbo compiler

`limbo -H module/sys.m` now emits independently computed C layouts for the
accepted int32/ref64 natural model, full map8 bytes and Frame64-shaped fixed
argument frames. It is an explicit metadata mode. Normal `-a`, Dis/AOT output,
checked-in runt.h, target defaults and runtime admission keep their existing
behavior. Generated declarations are written to stdout; use a private file and
check the command status before inspecting it.

```
MKBOOTSTRAP_LIMBO=1 sh makemk.sh
Linux/amd64/bin/limbo -H module/sys.m > /tmp/sys-native64.h
python3 libinterp/tests/native-consumer-layout.py --header /tmp/sys-native64.h --require-coherent
```

Use the actual installed native tool path on other supported bootstrap hosts.
Linux/AMD64 is the executed host in this checkpoint. This mode is Inferno's own
compiler option; it introduces no iyacc dependency or flag requirement for the
independent FlashVM Coco/R frontend.

## Independent layouts

The new limbo/native64.c reads verified Type/Decl identities and keeps a private
layout/map cache. It does not alter Type.size, Type.align, Decl.offset, old
Desc caches, signatures or bytecode constants. Pointer-bearing types occupy
8-byte slots; int/fix stay4, byte stays1, big/real stay8. Nested by-value fields
use natural alignment. Scalar-only four-byte aggregates remain four-byte values.
Reference maps retain the entire ceil(size/64) byte extent, including zero
suffixes and padding, with MSB-first eight-byte reference slots.

Named C aggregate definitions follow embedded dependencies, including forward
Limbo declarations. Anonymous tuples have their independently computed layout.
Channel aliases and named ADT channel fields emit payload types/maps, preserving
the established distinction between a Channel* field and its payload declaration.
Pointer cycles stop at their reference identity. Zero-sized aggregates, picked
ADTs, polymorphic calls, module data and unsupported C shapes are diagnosed
before declaration output.

Fixed frames contain native Frame links at0..31, an eight-byte result-address
field at32, reserved bytes40..63 and naturally aligned arguments from64. Their
full-size maps describe known argument references. Frame links/result addresses
and reserved temporary storage acquire no new GC ownership contract here.
Variadic functions emit their fixed prefix and a `_variadic` marker; variable
arguments, va-list layout and callable frame size remain unsupported.

No runtime Runtab or new Dis64 native-call signature is generated.
Opt-in private function records are described in [the Math consumer checkpoint](dis64-native64-math.md). `-H` rejects
`-t`, `-T`/`-d`, `-A`, `-S`, an explicit `-o` and multiple input files. Existing
`-t` still has its old16-byte table-map ABI; the new full maps must not be
silently copied into it. A150-reference argument fixture deliberately needs
20 map bytes and is emitted without truncation.

User C field spellings are preserved with byte-offset/size comments. Padding
and unnamed fields use the reserved `__limbo_native64_` prefix. Reserved field/
frame names and C keywords are rejected. A private identifier registry checks
flattened qualified names, typedefs, function prototypes, frame names, payload
names and descriptor macros for collisions before output. External consumer
namespaces still require their ordinary C compilation checks.

Private implementation limits are1MiB per object/frame,4096 cached types,
8192 generated identifiers and128 levels of by-value/C-spelling nesting. These
are compiler resource limits, not new wire ranges or extension/ABI IDs.
Allocation/I/O failure still requires the normal failed-command handling; the
stdout interface is not a transactional file installation service.

## Model check and representative progress

The emitted limbo_native64_model predicate checks scalar/pointer sizes, char and
IEEE-double properties, natural scalar/reference alignment and native Frame
link offsets. Call it before consuming the experimental metadata. WORD64 and
packed C structures are rejected by this predicate. A valid model alone does
not establish native callable ABI compatibility.

| WORD32 declaration | Explicit `-H` C / emitted metadata | Default `-a` C / emitted metadata |
|---|---:|---:|
| Sys_Qid bytes |16 /16|16 /16|
| Sys_Dir bytes |80 /80|80 /64|
| Sys_Connection bytes |24 /24|24 /12|
| Sys.sleep result address |32|16|
| Sys.sleep first argument |64|36|

The existing consumer auditor succeeds for its representative WORD32 and
WORD32+file selections when explicitly given the generated `-H` Sys header.
Its `full_native_abi_verified` stays false. The default checked-in or freshly
regenerated `-a` header still returns measured incompatibility status2 under
--require-coherent. No production header has been replaced. Include the normal VM lib9/isa/interp
declarations before this generated header, and include the generated declarations
once in a translation unit.

## Executed checks

```
python3 utils/tests/native64-layout.py
NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-layout.py
python3 utils/tests/native-limbo.py
```

The new driver builds a fresh real Limbo tool unless NATIVE64_LIMBO explicitly
selects an already rebuilt executable. O1 and O3/NDEBUG strict UBSan runs pass
14 actual declaration/map checks in each compatible model, including the shared
map8 validator, forward/nested aggregates, a recursive reference, byte/scalar
strides, anonymous tuples, channel payloads, Frame64 offsets, aliases and the
150-argument frame. There are49 active predicates in each compatible model and
one in each rejected WORD64/packed model:100 total per run. Headers are produced
deterministically by actual compiler invocations.

Fourteen early-rejection cases cover incompatible CLI selections, empty/picked/
polymorphic/data shapes, reserved/keyword names, deep nesting and a real
flattened-name collision. Rejected requests publish no declarations. The driver
is registered in the core checkpoint runner.

Existing native-limbo integration still builds four ELF64 tools and passes72
real-source Dis/AOT cases, ABI32 argument/result offsets, CLI failures and late
link preservation. Existing native-stub table tests and ten emitted strict C99
tables pass; the C-spelling suite passes its field/name/layout matrices and2000
emitted strict C99 declarations. JIT readiness remains unchanged.

## Remaining integration

This is a first explicit generated-metadata gate. Module constants, picked/
polymorphic consumers, complete native interface/header/table generation,
variadic tails, calling signatures, argument/result ownership and temporary roots
remain work. The generated header is not a drop-in replacement for full runt.h.
Actual builtin implementations have not been rebuilt/linked against these
experimental declarations. The compiler's ordinary Dis output still targets
its old ABI; a full new-format producer/runtime run is separate.

The new Dis64 loader keeps43 scalar opcodes/np0 and rejects new-format calls and
managed references. General inspector execution_supported remains false.
Complete emulator boot, concurrent/pressure GC, compaction and physical ARM64/
RV64 are not established here. Complex native handlers remain C. Shared wire,
registry, default NC64/SDK and existing JIT machine code are unchanged.

Build/import dependency: updated stubs.c links native64stubs even in a default
Limbo build. Import limbo/native64.c with the CLI/header/mkfile changes; the
native bootstrap wildcard already compiles it. Preserve matching limbo.y and
regenerated y.tab.c. The parser tables/y.tab.h and grammar productions do not
change; -H is only a compiler-driver branch.


## Complete emitted C geometry predicates

Each module now has a distinct generated predicate, for example
`limbo_native64_Sys_layout()` or `limbo_native64_Math_layout()`. Call it before
consuming that header's metadata. It first rejects an incompatible scalar/frame
model, then compares actual C sizes and field offsets against independently
computed literal extents. It checks named aggregates, type aliases, channel
payload declarations, fixed frames, result pointee sizes and argument fields.
Nested by-value members, including anonymous tuples, are checked through their
full enclosing member paths. Pointer referents are not followed as objects.
For a multi-module input each predicate validates the complete emitted header.
No pointer is dereferenced: member size expressions are unevaluated sizeof.

The separate names let standalone Sys and Math headers share one translation
unit; the existing guarded model helper is emitted once. Headers must still be
included once and ordinary duplicate source declarations remain C errors.
These predicates test geometry, not maps, native function signatures, calling
conventions, result/temporary rooting or reference ownership. In particular,
an anonymous result pointee has a checked size, without an independent field
reflection API. Existing fixture checks separately validate full map8 bytes.

The regression builds real Sys and Math declarations separately and together,
then executes their generated predicates under WORD32. A negative control
swaps the two equal-sized Pair fields in generated C while preserving the model
and aggregate size: compilation succeeds, model check succeeds, and the new
layout predicate rejects the changed offsets at runtime. Compatible fixture
models each execute53 active predicates and14 independent map checks; both
unsupported models now check the model and layout rejection paths. No builtin
implementation is rebuilt or invoked using these declarations in this gate.

[Private generated tables and binding](dis64-native64-binding.md) now match
selected actual C records before invocation and preserve qualified ADT method
names. This leaves shared call/ownership admission unchanged.
