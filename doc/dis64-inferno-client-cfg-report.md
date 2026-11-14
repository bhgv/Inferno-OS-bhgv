# INFERNO-0183 — file-client CFG and native-width module nil tests

Base: fdf0f83f7921a9687a51fbf2c03d18db5aa496da (0182).
Peer journal prefix4e345306/FLASHVM-0199 preserved.

Existing -64c importing clients now admit Word/Big/Real control flow, early
returns, if/else, loop backedges, break/continue and repeated LOAD inside loops.
One void argument-free init, one path/table and scalar interface signatures remain
required. No managed entry arguments, additional functions or generic heap graph
admission is implied.

The compiler independently checks module/nil operand declarations in its selected
client lowerer. Classic Word pointer equality becomes existing eight-byte BEQL/
BNEL, with symmetric operands normalized to the same MP0 module root/MP8 H cells.
Raw proof permits only that exact reference-read shape. Numeric Big/Real roles
remain distinct, and Word reads of those roots refuse. Public opcode/section/
signature IDs, file conventions, layouts and vendor pins do not change.

The writer now accepts the compiler's one nil constant in addition to the one
path, without changing the24-byte MP shape. Every branch target is relocated after
result-copy insertion. Earlier importing files remain byte-identical.

Raw proof first validates all instructions, exact call-island setup and immutable
nonoverlapping scalar spans. An island includes its normal result copy. Only its
start is a branch boundary. A separate CFG pass derives read/definition masks and
a preceding LOAD-attempt fact. Entry starts empty, joins intersect predecessor
outputs and a descending worklist reaches a fixed point through loops before
read requirements are tested. All boundaries must be structurally reachable;
validation is bounded by132*instruction-count queue pops. All64 scalar bits are
handled, including the second mask word. Analysis allocates no heap or VM owner;
refusal keeps output unchanged. Runtime construction/admission/GC/root/JIT lifetime
contracts remain the0182 ones. No termination proof is claimed.

## Evidence

- 15 CFG source pairs/1440 scheduler roots on each x64 and ARM64/Linux QEMU: nil
 equality/inequality in both source orders, early exit, both diamond arms, zero/
 one/many iterations, reload, break/continue, Big above32 bits and negative Real
 ordering. All four caller/provider C/RX combinations, stack growth, quanta1/3/1000,
 provider open/admission refusal and dontcompile outcomes pass exact results.
 Guarded failures now terminate normally. Client bytes are poisoned before startup;
 every client read/constructor allocation failure retires. Transport/GC seams in
 this matrix remain bounded fixtures.
- Complete consoles rebuilt and each executes60 new C/RX OS roots with actual
 namespace, heap, MP cloning, scheduler and unwinding. Every missing provider is
 handled by the compiled nil guard without Broken/panic. Two additional valid
 wrong-result provider controls take the command's conditional error path and
 retire normally with zero divide.
- 21 independent CFG raw images cover join definitions, zero-iteration loop entry,
 early exit before LOAD, every setup-interior target, nil-width/cell roles,
 high-half definition bits and64/65 spans. Join/load/boundary/high omission controls
 require semantic exit10, not crashes or sanitizer failures. Malformed code never
 executes. Both target gates and host ASan/UBSan raw checking pass.
- All15 constructor-only ASan/UBSan pairs pass without VM instruction execution.
 The known runtime-ASan instruction/unwind limitation remains open.
- The earlier28 raw inputs/initialized/result/map controls,17 linear source pairs/
 1632 scheduler roots and34 full-OS roots remain green on both targets. All17 old
 importing Dis64 outputs exactly match the prior compiler, and all15 CFG sources
 retain classic output bytes. The source writer's prior ten refusal/limit checks
 and scalar-call writer proof/rollback/three omission controls pass. The AWM body
 baseline remains14 accepted/346 refused with classic/traced bytes equal, not AWM
 execution.

Reproduction is integrated in native64-port.py. RU/EN guide:
[EN](dis64-client-control-flow.md), [RU](dis64-client-control-flow.ru.md).
The full-console results are Linux/QEMU evidence, not Android/Bionic GUI/device
or new concurrent filesystem/collector stress evidence.

## Peer coordination and next boundary

FLASHVM-0199/d7a0b28c received. Its base FP/SIMD copied diagnostics and scoped REAL
RX/fenv recovery remain peer-reported; no signal/debugger/quarantine code is imported.
Peer reports prior CI43622919835 success on six jobs; new FP CI43630606649 was still
in progress at that report. We do not convert it into a new pass claim here.
Vendor pin remains7f2241455734e1142a3254fad9d1ec4f94032ac7.

Our existing private MP/pool contract is unchanged; slot8's already constructed H
now participates in explicit full-width module nil equality. This assigns no new
managed type/kind ID and is not a generic managed DATA initializer agreement.
Next: ordinary init/context/list arguments and managed caller/heap/CFG support
toward AWM, then real Android/Bionic GUI/APK execution. A working AWM APK is still
unverified. Full managed signature/entry conventions require coordination rather
than silently assigning public IDs.
