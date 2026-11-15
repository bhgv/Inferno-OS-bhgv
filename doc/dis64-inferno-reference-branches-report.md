# INFERNO-0188 — native reference branches and Context constructors

2026-11-15. Base3aa7f03f / 0187. The selected client/Command file path now admits
ordinary Context field nil conditions, compatible reference identity comparisons
and Context constructors previously blocked by pointer-temporary reuse. No new
public ID, private signature version, native layout, handler, JIT emitter or
collector change is introduced. Command still binds via normal linkmod and calls
init explicitly. Private face0186 bytes and existing .m hash4244b354 are retained.

## Implementation

- Selected lowering converts reference equality/inequality to existing full-width
 IBEQL/IBNEL. This includes direct Context/list/field aliases and fused
 `ctxt.display/screen/wm == nil` or `!= nil`, with either nil operand order.
 Context owner, complete field layout, readonly nil role and branch ownership
 are checked. The field comparison reuses geometry proof without emitting an
 artificial temporary or adding an instruction; existing Dis-PC/CFG identities
 remain intact. Short-circuit conditions and loops keep their original edges.
- Local framing/encoding adds the existing indirect-source comparison operand
 order: middle, source base, field displacement, target. The current constructor
 already projects that source into Inst.s.i.f/s.i.s. General scalar decoding
 stays unchanged. Raw proof checks complete mode, nil MP+8, exact field offsets,
 mapped Context owner and structural branch boundaries before any execution.
- Direct reference pairs require two compatible mapped roots. The mapped role,
 rather than LONG opcode/mode alone, distinguishes these from numeric big
 equality, which uses the same instruction representation. Mixed numeric/root
 and incompatible root classes refuse. Independent raw fixtures include valid
 numeric LONG equality beside pointer comparisons to guard this distinction.
- Classic temporary allocation reused any same-sized pointer slot. A Context
 constructor could therefore inherit a List declaration and collide with the
 immutable root-class proof. Selected -64c/-64b allocation now keeps Context
 temporaries separate, including the in-place candidate and later reuse in the
 opposite direction. Existing non-Context reuse and ordinary classic allocation
 remain unchanged. Existing INEW/MOVP lower all-nil or reference-valued Context
 initializers through the already checked descriptor24/E0 and field roles;
 no DATA initializer profile or extra wire descriptor is added.

## Evidence

-23 new pairs:15 reference/branch cases and8 constructor cases. Direct nil fields,
 alias/different Contexts, alias/different lists, field/channel aliases, caller
 predicates, short-circuit nil guards, loops, nil dereference, non-nil field
 references, mixed nil/non-nil fields and native-loop clear/restore are covered. Constructors run in local
 declarations, assignments, reloads, callees, repeated replacement and loops.
 A native-valued constructor copies three distinct opaque owned field witnesses
 and checks typed identity; these are not Display/Screen/channel resources.
- All48 Command pairs/4608 scheduler roots pass per x64 and ARM64 Linux/QEMU,
 including six separately native-seeded inputs. Both caller/provider C/RX,
 stack growth, quanta1/3/1000, open/admission refusal and dontcompile execute with
 actual gc.c epochs between nonterminal quanta. All positive roots/types/MPs/
 modules/arena owners retire, and source bytes are poisoned before startup.
- The isolated actual BEQL/BNEL C-handler oracle checks16 predicates on eight
 value pairs, including unequal high halves with identical low32, nil-pattern
 bits and equal values. Patterns remain unowned value bytes, never heap
 addresses/GC roots or a malformed guest image. Replacing these calls with the
 real WORD handlers must return semantic exit10, not crash/sanitizer output.
 This width oracle is distinct from the heap-owned C/RX source-file matrix.
- Context-map, source/destination offset and callee-frame omission controls pass
 on both targets. Offset controls retain the earlier native exact-field fixture
 and preserve prepared argument offsets. Callee-frame omission additionally
 covers a newly constructed private Context. Collection controls stop before
 guest execution resumes after an observed object is reclaimed.
-41 new raw images cover fused field comparisons and direct root pairs in both
 executable/provider files: offsets, owner/maps, wrong nil source/mode, WORD
 truncation, incompatible classes, prefix/tail/numeric roles, invalid targets
 and branches into the argument island. Five pure substitutions weaken offset,
 nil, owner, root-class or mapped/numeric selection; each causes semantic exit10.
 The selection control rejects the valid mixed numeric/reference fixture, making
 opcode-only classification observable. Malformed files never execute. Host
 ASan/UBSan and ARM64 pass. Previous38 field and41 Command raw checks/all controls
 also pass; refusal leaves output views untouched.
- Constructor-only host ASan/UBSan passes all48 pairs with every provider
 read/construct/link allocation refusal enumerated. Whole-VM ASan and concurrent
 collection are not new claims.
- Rebuilt complete x64/ARM64 consoles pass168 main Command roots each (42 entirely
 source-owned pairs, C/RX with valid and physically missing files), plus hash/
 default/entryless controls and updated RU/EN examples. Normal callers assert
 real provider load success. All214 prior main console roots pass again.
- All62 earlier scalar clients/5952 roots also pass with actual collection:
 combined110 pairs/10560 roots per target. Previous28/21/64/26 raw groups and all
 controls remain green. The compiler emits96 deterministic Command files; old62
 scalar Dis64 bytes match0185, old26 Command bytes match0186 and old50 Command
 bytes match0187. New files accepted by a prior compiler also remain identical.
 All158 compared classic outputs match. Five well-typed unsupported sources
 refuse without payload, including a constructor inside prepared call setup.
- Actual AWM preflight remains14 accepted/346 whole-body refusals with unchanged
 classic bytes. These selected adapters do not broaden generic AWM lowering.

Updated native64-port.py includes the new raw gates and semantic width control;
the normal Command matrix automatically covers all48 pairs. Reproduce with
utils/tests/dis64-command.py, dis64-command-boot.py, dis64-command-producer.py,
dis64-reference-branches-proof.py and dis64-reference-width.c. Tutorial:
[EN](dis64-command-init.md), [RU](dis64-command-init.ru.md).

## Remaining boundary and peer state

Construct a Context into a local before calling init: an allocation inside
prepared argument setup still refuses. Nested/dual field dereferences, arbitrary
ADT/array payloads, general DATA, String content comparison, channel operations,
additional imports/exports and real Draw/Tk resources remain. AWM and a live
Android/Bionic GUI APK/device run remain ahead; Linux/QEMU is not that evidence.

At checked experiment3aa7f03f there is no new peer post beyond FLASHVM-0202/0203
received in0186. Their source/runtime is not imported. Vendor stays
7f2241455734e1142a3254fad9d1ec4f94032ac7, and no new shared wire ID is proposed.
