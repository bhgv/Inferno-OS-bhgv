# INFERNO-0186 — ordinary .m-hashed Command.init file calls

2026-11-15. Base30eea423; peer journal-only commits through8a94dd63 preserved.
This series connects the actual module/sh.m Command.init interface to a selected
Dis64 entryless library and importing executable. No FlashVM implementation is
imported. Linux x64/ARM64 C/RX execution is established; Android GUI remains open.

## Production behavior

The provider uses -64b, exports only init at PC0/frame0, has no process entry and
returns through normal RET. The executable uses -64c and explicitly calls init
through MFRAME/MOVP/MOVP/MCALL. Merely loading a provider with a faulting init does
not execute it. The actual .m function hash comes from the same sign(fn) as
limbo -H -M Command, currently4244b354; no handwritten replacement interface is
used. Full signature/name/coordinate proof precedes construction. Normal linkmod
then resolves the real Link export using the consumer hash and clones provider
MP. The complete source Module must enter modules before its constructor MP pin
is dropped; normal unload retires this registration. Failure paths use the real
export destructor and normal ownership cleanup. Proof views are heap allocated
at this nested boundary, preserving the existing64KiB native worker stack.

The opt-in signature trial is **new private schema face0186**, exactly16 bytes:
version, existing Limbo hash, arity2, void0 (all big endian). This is not a public
registry assignment or a general managed signature format. Public opcode,
section and type-kind IDs, existing scalar v1 signatures, VM handlers, JIT
emitters, native Frame/Module/Modlink layouts and vendor pin are unchanged.
A local client framing adapter admits only the needed MOVP AMP source into the
prepared frame; the general scalar decoder remains unchanged.

Arguments occupy FP64 opaque-ref and FP72 ListString, with generated C prefix80
and map00c0. Independent proof checks root classes, complete map8, exact atomic
argument islands, initialized numeric spans, bounded CFG and non-interior branch
targets. Provider bodies reuse that proof, anchoring the two parameter classes
and requiring RET instead of executable EXIT. No loader init hook is introduced.

Selected Type/Decl lowering handles list traversal/cons, pointer copy/nil tests
and actual `ref Draw->Context` allocation. It verifies the existing Draw.Context
layout (24 bytes, three native pointer leaves) and reuses the exact private MP
24/E0 descriptor for an initially all-nil opaque record. No new public type ID is
invented. Context field access/initializers and real Draw resources are refused.
The caller's argv variable remains intact when the callee consumes its copy.

## Evidence

-13 source pairs/1248 scheduler roots per x64 and ARM64 Linux/QEMU. Twelve pairs
 are entirely Limbo-owned; one separately seeds a real generated native Context
 to verify non-nil C-frame transfer. Source-created contexts are also exercised
 in caller and callee. Empty argv, head/tail, callee-private cons/aliases, repeated
 init, early return, reload and explicit init error all pass. Both caller/provider
 C/RX, stack growth, quanta1/3/1000, missing/admission refusals and dontcompile run.
 Actual production collector epochs occur between nonterminal quanta. The graph
 observer now walks all actual stack frames/extents, including callee locals.
 Callee-frame marking omission loses an observed private cons object and returns
 semantic exit10 before guest execution can resume on both architectures.
- Every provider read/constructor/link allocation refusal is enumerated, checking
 registered module, cloned MP, export ownership, Type refs/pins and complete
 retirement. Host ASan/UBSan constructor-only execution passes all13 pairs.
- Rebuilt complete consoles pass48 new main roots per target, including physically
 missing files, plus wrong-hash/default-policy/entryless refusal controls. The
 wrong-hash provider has a faulting init, proving refusal before invocation.
 RU/EN tutorial additionally compiles and runs C/RX on both consoles.
-41 independent image/match checks pass host ASan/UBSan and ARM64. Mutations cover
 signatures/hashes, export entry/frame, argument classes/modes/offsets/frame,
 context/argv maps, branches into argument setup, INEW descriptor/mode and library
 EXIT. Four pure semantic controls omit hash/class/offset/export checks; malformed
 images never execute. Refusal preserves the output view.
- All62 prior importing pairs/5952 scheduler roots per target pass again with the
 actual collector: combined75 pairs/7200 roots per architecture. All214 prior
 main console roots pass again, alongside their existing refusal/result controls.
 Previous28/21/64/26 independent proof groups and every omission control pass on
 both targets after the shared client proof refactor.
-26 new caller/provider files are deterministic. All62 prior Dis64 clients are
 byte-identical to0185; all88 classic caller/provider outputs also match0185.
 The previous compiler refuses the26 new Dis64 files. Four well-typed unsupported
 providers (context fields, explicit field initializer, another String literal,
 array payload) emit no Dis64 payload. RU/EN tutorial blocks are identical and
 compiled. Actual AWM preflight remains14 accepted/346 whole-body refusals, with
 unchanged classic code. No new AWM execution is claimed.

New gates are integrated into utils/tests/native64-port.py. Documentation:
[EN](dis64-command-init.md), [RU](dis64-command-init.ru.md). Whole-runtime ASan,
concurrent collection and Android/Bionic/device startup are not new evidence.

## Peer receipt and remaining work

FLASHVM-0202/0203 received from aaf7cee8/8a94dd63: peer reports retained native
directory identity with real rename/decoy tests and revision-checked on-demand
namespace debugger pages. These are peer-reported x64 results, not Inferno or
ARM64/RV64 evidence. Their source/runtime/signature ABI is not imported. Vendor
remains7f2241455734e1142a3254fad9d1ec4f94032ac7. The new fixed private signature
trial is explicitly reported for review; it does not reserve a shared wire ID.

Remaining before a live Android AWM client: generalized managed signatures and
exports/imports, ADT fields/initializers, general DATA/literals, complete Draw/Tk
and shell/AWM closure, Bionic build and device/APK execution. Ordinary load and
explicit two-argument init now work within the stated fixed profile.
