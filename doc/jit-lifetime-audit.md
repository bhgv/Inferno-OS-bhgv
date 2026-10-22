# JIT helper lifetime, call range and helper handoff

This extends the IA-32/ARM32 publication audit without changing the VM ABI.

## Type helpers outliving module code

Type descriptors have their own reference counts. Module destruction releases
its Type references and frees its native program separately; keeping a Type
alive does not keep that program alive. Generated destructors nevertheless
called the creating module's MacFRP. Reusing such a Type after that program
was freed could jump into freed code.

Both backends now append an independent reclaim thunk to the permanent comvec
allocation and publish its address only after cache synchronization succeeds.
Type destructors call that address rather than a module-local macro. The
existing per-module macro remains available for module-local operations.
The permanent block is 256 bytes on IA-32 and 128 words on ARM; generated
bytes, not the entire capacity, are synchronized. It remains process-lifetime
storage, as comvec already was. Type-owned code still dies with the Type.
Unpublished blocks remain covered by the compilation rollback boundary.

## Full-address ARM helper calls

CALL/CCALL previously encoded a BL displacement without checking its +/-32 MiB
range. C functions and persistent Type code are not guaranteed to be nearby.
They now load a full 32-bit literal target, set LR to the continuation and use
BX on interworking-capable targets. Forced literal emission keeps pass sizes
independent of the target's value. Pre-Thumb ARM architecture builds use MOV PC
instead, preserving their A32-only instruction set. Conditional calls preserve
flags/LR when not taken. Internal CALLMAC/Dis branches are unchanged; this is
not general branch relaxation for arbitrarily large modules.

## Correct exception PC and canonical state at C boundaries

The native helper bridge only saved R.PC when each caller explicitly supplied
WRTPC. Many arithmetic, conversion and allocation helper sites omitted it,
although those helpers can raise. handler() selects a compiled exception range
using R.PC - module_base - 1, so a stale PC could select an older range.

punt now identifies real instructions in the module and saves the next native
PC before operand preparation. This also precedes a fault while resolving an
indirect operand. Synthetic macro instructions, such as RET fallback, retain
their existing explicit-PC behavior and are not indexed as module instructions.
Both patch arrays are cleared on allocation so forward PCs read during the
sizing pass are initialized. The publication tests now poison ordinary malloc
storage and implement mallocz's clear flag rather than always returning zeros.

## Executed regression checks

```
python3 libinterp/tests/jit-lifetime.py
python3 libinterp/tests/jit-handoff.py
python3 libinterp/tests/arm-calls.py
CFLAGS='-O1 -g -D__ARM_ARCH=4 -fsanitize=undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/jit-lifetime.py
```

These tests require Unicorn and a host C compiler. UBSan is enabled by default.

- jit-lifetime runs 96 initializer/destructor cases across IA-32 and ARM with
  production Type, preamble and reclaim-thunk encoders. Former module memory
  is unmapped before execution; call targets are not rewritten. It checks nil
  and shared children, real native fast reference decrements, slow final frees,
  volatile-register clobbers, canonical FP/MP reloads, frame relocation and
  allocation canaries/balance. A separate legacy ARM profile passes as well.
- jit-handoff runs 104 cases using production punt, operand encoders and literal
  pools. It checks direct/module/immediate/indirect/implicit middle operands,
  current PC before helpers and indirect operand faults, normal return, NEWPC,
  TCHECK yield/continue, saved IA-32 registers, volatile clobbers and FP/MP
  reloads. Synthetic macro PCs are checked separately. The error case stops
  at the callback boundary and checks an independent exception-range oracle;
  it does not execute the full VM handler or longjmp path.
- arm-calls runs 2,880 cases covering all used condition codes and NZCV states,
  near/far/high 32-bit addresses, A32/Thumb targets, returns, two-pass sizing and
  the pre-Thumb ARM profile. Untaken calls preserve flags/LR. The callee is a
  tiny native return stub rather than an actual target C library.
- x86-types also executes unchanged native CALLs at their encoded virtual
  addresses after removing its former module block; its permanent target is
  a recording stub. The lifetime test above exercises the real reclaim thunk.
- Production publication/rollback tests pass with 52 IA-32 and 50 ARM VM-error
  event points after adding permanent thunk generation. Poisoned allocation,
  allocation/cache failure, retries and shared Type restoration still pass.

Negative controls were run separately for each architecture: restoring calls
to module-local reclaim code makes the lifetime test fault on an unmapped
fetch; suppressing automatic PC publication makes handoff fail its stale-PC
check. Restoring direct ARM BL also fails the far-A32 call case.

All 28 libinterp Python suites pass, including the existing arithmetic,
loader, publication and native-encoder suites. Full xec/IA-32/ARM syntax checks
pass with Linux headers, including the legacy ARM preprocessor profile.

The native tests use projected 32-bit REG/Heap layouts and simulated C bodies.
They exercise actual generated call/return/reclaim instructions, but do not
run the target C compiler's prologues, full GC/pool allocator, Loader/unloader
integration or an OS boot. Code unmapping is a host emulation test, not a
hardware unload test. Executable-memory permissions, physical cache coherence,
large-module Dis branch reach and performance remain outside this coverage.

IEEE IA-32 branch semantics and overlapping block moves are covered in
[jit-float-move-audit.md](jit-float-move-audit.md).
