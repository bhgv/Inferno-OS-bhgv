# Captured calls, returns and decoded module/process operands

`call` must capture both the frame pointer and target before writing either frame
link. A target stored in `Frame.lr` or `Frame.fp` otherwise changes before the
jump. Source and target slots may themselves overlap frame links. Frame header
access uses byte offsets and `memmove`, including on hosts with eight-byte native
pointers and only four-byte VM slot alignment.

The x86-64, arm64 and RV64 decoded-operation leaf backends now compile `ICALL`.
They capture the frame, target, old PC and old FP before publishing either link,
then update REG.FP and REG.PC. No allocation, native stack, helper call, reference
change, or Dis-code relocation is introduced. Module.compiled stays unchanged.
Private layout adapters pass explicit `Frame.lr` and `Frame.fp` offsets. Invalid
or overlapping field offsets decline emission. Nil/H frames decline execution
before even reading the target slot. RV64 uses paired word stores for frame links
and declines frames not aligned to four bytes; the C fallback handles them.
RV64 `ICALL` also works without multiplication/division extensions. Operand slots
on RV64 retain the existing minimum four-byte alignment requirement.

Related C fixes cover six instructions: `call`, `mcall`, `load` (`iload`), `spawn`,
`mspawn`, and `ret`. Module/process operands use alignment-safe copies. `mcall`
rejects nil/H frames before publishing registers or acquiring references and
captures builtin-return links before cleanup. `ret` captures all frame links
before changing registers or releasing frame storage and rejects nil/H current
frames. The existing root-frame return exception remains unchanged.

## Drivers

```
python libinterp/tests/call-slots.py
CALL_NATIVE=1 python libinterp/tests/call-slots.py
python libinterp/tests/return-slots.py
MODULE_CASES=calls python libinterp/tests/module-types.py
python libinterp/tests/module-imports.py
STACK_CASES=spawnalignment python libinterp/tests/stack-startup.py
SPAWN_SUCCESS_ONLY=1 STACK_CASES=spawnalignment python libinterp/tests/stack-startup.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=call python libinterp/tests/leaf64-jit.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=frameabi python libinterp/tests/leaf64-jit.py
python libinterp/tests/slice-tokenize-suite.py --core
```

The C fixtures extract production functions and use real declarations (except
the existing minimal import loader fixture). Native call execution requires a
Linux x86-64 host. WORD32 fixture models retain native host pointers and do not
claim a complete 32-bit-pointer runtime.

* C calls: 2,576 checks per word model, including all byte alignments of slots
  and frames, exact target/source aliases with frame links, preserved surrounding
  bytes and nil/H rejection before an inaccessible target.
* Native x86-64 calls: 5,136 checks per model, running the same independent image
  oracle through C and the emitted leaf. No opcode helper substitutes the native
  execution path.
* Returns: 195 checks per model, including module switching, cleanup that
  overwrites the frame, unaligned links and preserved state on invalid roots.
* Module calls: 163,840 WORD64 and 122,880 WORD32 checks, including every source,
  index, module-slot and frame alignment; normal/builtin/compiled transitions;
  full-width valid/invalid imports/exports; nil/H frames and balanced references.
* Loads: 8,203 WORD32 and 8,204 WORD64 cases with independently aligned slots,
  self/external paths, destination aliasing, failed links and owner counts.
* Spawn alignment: 384 cases per scheduler/model (emu/os, WORD64/WORD32), with
  successful publication and both allocation failure points. Success-only mode
  covers 128 cases per scheduler/model.
* Three-backend call emission/execution: 11,110 checks including encoding,
  capacity, malformed offsets, high pointers, full data/REG images and fallback
  before inaccessible targets. Capstone decodes the emitted code; Unicorn
  executes it. This is distinct from hardware arm64/RV64 acceptance.

The new C/native call, return, module-call and import matrices pass ASan/UBSan
with stack instrumentation. Spawn success alignment passes ASan/UBSan; the full
spawn failure matrix passes UBSan. Its ASan run reports stack redzones following
nested `krethrow`/`longjmp`, like the existing startup failure families; it is not
counted as passing ASan. No stack instrumentation is disabled to hide this.
The fixture's existing LeakSanitizer hook only disables process inspection.

Parent-source overrides (`CALL_SLOT_SOURCE`, `RETURN_SLOT_SOURCE`,
`MODULE_TYPES_XEC_SOURCE`, `XEC_SOURCE`, `STACK_XEC_SOURCE`) reproduce the original
aliasing, nil-frame and alignment failures. Hardware boots and a complete
native-call/return JIT for every opcode remain separate work.
