# INFERNO-0187 — checked Draw.Context pointer fields

2026-11-15. Base3d4997bc (0186). The selected importing-client/Command provider
now transports the actual Draw.Context display, screen and wm fields. Existing
private face0186 signatures, public IDs, native structs, handlers, JIT emitters,
collector and FlashVM vendor pin are unchanged. No peer runtime is imported.

## Implementation

Type/Decl lowering checks a frame reference to the actual named Draw.Context,
its complete three-leaf layout and root kinds (ref,ref,chan). Existing classic
field offsets0/4/8 become native0/8/16 only after that check. Each operation has
one exact indirect Context operand and one direct mapped reference or readonly
nil. Nested/dual indirect operands and scalar field operations refuse. A small
owned compiler sidecar holds displacements and is freed on success/refusal.
The selected lowerer retries unsupported direct-body operand roles only after
successful storage planning; every instruction is revalidated in the selected
lowerer and final raw-file proof. General typed-body admission is unchanged.

The local client framing adapter decodes existing MOVP indirect-source order;
the shared scalar reader remains unchanged. The constructor now fills source
Inst.s.i.f/s.i.s as well as its existing destination projection. Source and
destination secondary offsets are exclusive, so no ambiguous dual projection
is admitted. Compiler-produced bytes are independently checked before writing.

Raw proof assigns disjoint internal Display, Screen and wm root classes. Only
a Context-class root owns these fields, only offsets0/8/16 are legal, and an
unmapped/prefix/reserved-tail local cannot receive a field reference. Read/write
values must match the field class; nil is allowed. A field cannot become Context,
String or ListString. Existing bounded alias propagation, all-path numeric facts,
GC map checks, CFG/call-island guards and normal MOVP ownership remain in force.
These classes and compiler sidecar are not serialized type IDs or capabilities.

A real incremental-build omission was found: load64.o did not depend on the
selected proof/constructor headers. libinterp/mkfile now declares these inputs,
and the command binding input of readmod.o. Rebuilt logs show actual load64.c
compilation on both targets, not only archive replacement/relinking.

## Evidence

-12 new field pairs cover reads, clear/restore, caller-side access, aliases,
 two Context objects, a direct typed parameter initializer, loops, callee cons
 locals, nil-read/nil-write errors and non-nil opaque reference transport.
 The last uses an actual generated native Context with three distinct owned
 opaque heap witnesses; these are not Display/Screen/channel resources.
 Native sizeof/offsetof checks establish Context24 and field offsets0/8/16,
 plus existing Command frame arguments64/72. Independent post-call identity
 checks require each original field owner, even when every value is non-nil.
- All25 Command pairs/2400 scheduler roots pass on x64 and ARM64 Linux/QEMU:
 previous13 plus new12, with two separately native-seeded cases. Caller/provider
 C/RX, stack growth, quanta1/3/1000, load/admission failures and dontcompile run.
 Actual gc.c epochs execute between nonterminal quanta. The independent graph
 observer additionally follows ordinary mapped heap records, so Context child
 objects are checked rather than merely the record pointer.
- Context-map omission detects a reclaimed observed child before guest execution
 resumes (semantic exit10). Separate safe constructor controls force source or
 field-destination displacement to zero; all accesses remain in the Context and
 all references are valid opaque owners, but exact native identity must fail.
 Prepared-frame argument offsets are preserved by the destination control.
 A callee-frame omission is also checked with fields plus callee-private cons.
 Both architectures pass these controls. Crash/sanitizer output is not detection.
-38 raw images cover both client/provider fields: invalid/misaligned/negative
 offsets, wrong owner, MP/prefix/argv bases, unmapped/reserved-tail destinations,
 scalar/immediate/path/list stores and Display-to-Screen confusion. Three pure
 guard omissions (alignment, Context owner and field-role matching) return
 semantic exit10 without executing malformed code. Host ASan/UBSan and ARM64
 pass; refused output views remain unchanged. Previous41 Command image/match
 checks and all four controls pass again.
- Host constructor-only ASan/UBSan passes25 pairs, including every provider
 read/constructor/link allocation refusal. Whole-VM ASan and concurrent GC are
 not new claims. The final native-seeded case passes again after explicit
 generated field offsetof assertions.
- Complete rebuilt consoles pass92 main Command roots per target (23 all-source
 pairs, each C/RX with real and physically missing providers), plus existing
 wrong-hash/default/entryless controls and the updated RU/EN tutorial C/RX.
 Normal-path callers now assert that a valid provider actually loads; a quiet
 nil-returning importer cannot mask admission failure. Missing-file callers
 retain their graceful nil return. The prior214 main console roots also pass.
- All62 previous importing pairs/5952 scheduler roots pass again with the real
 collector. Combined matrix:87 pairs/8352 roots per architecture. Previous
 28/21/64/26 raw proof groups and every control pass on both targets.
-50 Command caller/provider files are deterministic. Existing62 scalar Dis64
 clients match0185; old26 Command files match0186, and compatible new no-field
 callers also match it. All112 compared classic outputs are byte-identical.
 Four well-typed unsupported providers refuse without payload (nested Display
 access, Display allocation, extra String literal and array payload).
 RU/EN examples are identical, compile and execute. Actual AWM preflight remains
 14 accepted/346 whole-body refusals with unchanged classic bytes; generic AWM
 lowering is deliberately not broadened by the selected file-client adapters.

Gates are integrated in native64-port.py. Reproduce with dis64-command.py,
dis64-command-boot.py, dis64-command-producer.py and the new
utils/tests/dis64-context-fields-proof.py. Working example:
[EN](dis64-command-init.md), [RU](dis64-command-init.ru.md).

## Remaining Android boundary

This admits field reference transport and normal ownership, not GUI resources.
Explicit locals are required around field operations; fused field comparisons,
indirect-to-indirect copies, deeper ADT reads, channel operations, general
constructors/initializers and additional imports/exports remain outside the
profile. A direct parameter assignment with all-nil Context fields is covered;
constructor expressions introducing an untyped temporary can still refuse.
Draw/Tk services, full shell/AWM closure and live Android/Bionic APK/device
execution remain. Linux/QEMU results are not Android GUI acceptance.

FLASHVM-0202/0203 remain received as reported in0186. No new peer exchange was
present at the checked experiment head. Vendor remains
7f2241455734e1142a3254fad9d1ec4f94032ac7; no new shared wire/schema ID is proposed.
