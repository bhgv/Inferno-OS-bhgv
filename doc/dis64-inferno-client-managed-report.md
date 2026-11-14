# INFERNO-0185 — mapped String/List clients and actual collector execution

2026-11-15. Extends the existing importing executable profile with mapped locals,
not new managed import/export kinds or a special loader-init rule. Production
changes are confined to independent client proof, selected client lowering and
its constructor's result-binder selection. No VM handler, JIT emitter, collector,
public opcode/kind/signature, Module/Modlink/Frame layout, private MP/pool layout
or vendor revision changes. General typed-body admission is unchanged.

## Production path

- The compiler admits String/List/nil locals, emits their map8 bits and lowers
 CONSP/HEADP/TAIL only in the selected importing-client mode. Existing MOVP/LENC/
 LENL are retained. List/module nil comparison derives real reference roles and
 selects existing eight-byte equality instructions, with remapped Dis PCs.
- Independent proof declares mapped FP roots before numeric spans. No prefix or
 reserved tail root is allowed. MOVP class equalities reach a bounded fixed point
 before opcode effects/CFG checking. Internal classes distinguish String and
 ListString; their enum values never enter signatures or wire Type IDs. Producers
 are closed: owned filename/nil, reference copies, pointer cons/head/tail. Extra
 literals, arbitrary pointer/descriptor payloads and nested lists remain refused.
- All mapped frame slots start H through ordinary frame initialization and seed
 only the root definition facts. Numeric reads retain all-path initialization.
 Immutable kind/spans forbid numeric/root overlap and String/List confusion.
 Limit64 now counts numeric spans and roots together. No new heap allocation or
 recursion is introduced in proof; alias solving is capped at65 passes over256
 instructions before the prior bounded CFG solver.
- A caller containing mapped locals uses the existing native64typedlea8step.
 Its result LEA carries an internal eight-byte unmapped-tail contract added by
 the checked constructor, not the serialized code. The binder checks the caller
 map, prepared callee/stack geometry and complete native result address. The prior
 scalar-only binder remains unchanged. Owned roots and cleanup follow normal VM
 reference counts and the existing collector.

## Evidence

- 12 new source pairs/1152 scheduler roots per x64 and ARM64/Linux QEMU: String
 lengths/nil/aliases, empty lists, cons/head/tail, shared tails, nil String heads,
 construction/walking loops, full-width nil branches and head/tail nil errors.
 All caller/provider C/RX combinations, growth, quanta1/3/1000, provider open/
 admission refusal and dontcompile pass. Every client constructor/read allocation
 refusal retires. Client bytes are poisoned before startup.
- Actual production gc.c runs three complete color epochs between nonterminal
 quanta in a sweepable arena. An independent observer snapshots reachable frame
 roots/list tails/pointer heads and checks survival before the next instruction.
 Frame-mark and list-traversal omission controls produce semantic exit10 after
 an observed object is reclaimed, before execution can resume. Both controls pass
 on both architectures. All native owners, roots, descriptor refs and arena slots
 retire. I/O/transport seams are fixture-backed; collection is synchronous between
 quanta, not a new concurrent collector proof.
- The prior50 source pairs/4800 scheduler roots per target also pass with this
 actual collector. Combined matrix:62 source pairs/5952 roots per target.
- Rebuilt complete consoles pass48 new main OS roots per target: actual namespace,
 heap/MP, scheduler and unwinding, including graceful physically missing files and
 existing dereference-of-nil errors. Wrong-result provider/default-policy controls
 pass. All166 prior main OS roots pass again (34 linear,60 CFG,72 numeric), with
 their prior controls. The RU/EN tutorial agrees, compiles and runs C/RX in both
 complete consoles.
- 26 raw images include unmapped/prefix/tail roots, type confusion, unauthorized
 MP/prepared-frame access, unsupported list payloads, initial nil aliases and a
 sixteen-root alias chain. Four semantic controls weaken class matching, result-
 tail exclusion, H seeds or alias convergence. Host ASan/UBSan and ARM64 pass;
 malformed images never execute. Constructor-only host ASan/UBSan passes12 pairs;
 whole-runtime ASan remains outside this claim.
- The50 prior Dis64 files remain byte-identical to the0184 compiler; all62 classic
 files match it. New files are deterministic, cover all six managed opcodes and
 refuse in the prior compiler. Four well-typed unsupported source examples emit
 no payload. All12 executed managed images exactly match the final compiler after
 restricting new list lowering to the selected client mode. Previous28/21/64 raw
 proof groups and all their controls pass on both targets. AWM diagnostic coverage
 remains14 accepted/346 refused whole bodies with classic bytes unchanged.

Gates are integrated in native64-port.py. Reproduce with `--managed --gc` on
scalar-client.py, `--managed` on scalar-client-boot.py, and managed-proof.py /
managed-producer.py. Working example and precise restrictions:
[EN](dis64-client-managed.md), [RU](dis64-client-managed.ru.md).

## FlashVM and next boundary

FLASHVM-0201/6f2236c7 received: frozen namespace load plans/opened-file provenance,
atomic replacement/cache tests and hosted identity fix are peer-reported. The
journal prefix is preserved; none of those source changes is imported and no
zero-identity defect is inferred in Inferno's real filesystem. Vendor pin remains
7f2241455734e1142a3254fad9d1ec4f94032ac7.

This series establishes managed locals/heap/collector behavior around scalar
file calls. Ordinary exported Shell.init(context,argv), managed argument/signature
matching, context ADT access, general literals and the AWM closure remain pending.
Init remains a normal function called by sh after load. Linux/QEMU results do not
establish Android/Bionic GUI/device startup or a working AWM APK.
