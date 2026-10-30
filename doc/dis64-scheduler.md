# Dis64 scalar scheduling and terminal cleanup

The experimental constructor still returns an unpublished Module. The connected
scheduler fixture now passes that owner to the actual schedmod/newprog path and
executes the process's actual xec entry in C and AMD64 RX configurations.
The [experimental file route](dis64-fileloader.md) separately connects actual
readmod/parsemod to these components. Default routing and a complete emulator
boot remain unchanged/unverified.

## Root admission fix

Previously schedmodule checked only entry-frame size and the stack-size hint.
It could clone MP and publish a runnable Prog whose bytecode entry was nil,
one-past the program, or misaligned relative to its original Inst array. The
failure would be deferred until execution. Invalid entry-type representations
could also be dereferenced before a usable frame was established.

include/vmstartup.h now validates the original bytecode program span, entry PC
and basic frame metadata before cloning or publishing. Relative alignment allows
packed instruction images; opaque native-PC entries keep their existing meaning.
Frame pointer-map semantics are still checked by the existing stack allocator.
Both emulator and native-kernel schedulers use the same admission helper.

schedmod consumes a valid Module reference on success or protected failure.
A nil, sentinel, misaligned or wrapping owner representation has no reference to
consume: the public entry rejects it before installing failure cleanup state.
The caller must still supply actual readable owned objects; arithmetic checks
are not a host-memory mapping probe.

## Actual connected production paths

`python3 libinterp/tests/dis64-scheduler.py` uses unchanged production bodies for:

- Dis64 prepare64pilot, Type/heap ownership, initializer application and compiler;
- linkmod/newmp, independent instance MP, complete root-frame/stack allocation;
- schedmod/newprog/newgrp, PID hash, process lists and run-queue helpers;
- full dec.c, xec, C handlers/native entries and actual IEXIT/DIVW error handling;
- emulator error/kprotect, progexit, broken-process retention, destroystack,
  delprog, group release and Module/Type cleanup.

The test uses the same 180-byte MOVW42 and 192-byte initialized ADDW2 goldens,
plus division-by-zero and invalid-shift images. Each runs repeated load/start/execute/exit cycles
through C and native AMD64 paths. Counting wrappers call the actual generated leaves
and unchanged C bodies: per run24 native calls (12 accepted/12 declined),
60 C-handler calls and12 actual complete-tail bridge calls. Zero division and
invalid shifts decline before publication, then reach their authoritative C errors. Instance MP is retained before terminal cleanup;
only that live owner is read afterward. The original Module MP is a separate
clone source, and is never mistaken for the process result.

The fixture captures the actual 0xdeadbeef Prog marker before its allocation is
freed. It then checks absence from PID/process/run queues, balanced resource
references and allocator guards. A failed second startup leaves an existing
runnable process unchanged. Two-root cases verify that exiting the first root
does not issue final cleanexit while the second remains runnable. Broken roots
remain inspectable until explicitly resumed for terminal cleanup.

Every tracked startup allocation is failed in turn. Invalid entry/frame/program
metadata must fail before any startup allocation and preserve existing scheduler
roots. The pre-fix scheduler fails this rejection regression on a nil entry.
A valid opaque native-PC entry is tested for admission only, never executed.

## Evidence boundary

Default and O3/NDEBUG strict UBSan runs pass four images, 70,960 active predicates,
48 actual terminal process deletions and40 final cleanexit requests per run.
The existing emulator/kernel stack-startup matrix passes10,432 cases. Complete
object compilation covers20 core/scheduler files in each scalar model (40 objects).

Host workers, acquire/release blocking, allocator pools, namespace/file/environment
closure and final OS exit remain fixture boundaries. Root environment setup is
performed at the disinit boundary before execution. There is no live concurrent
worker, full vmachine exception-selection loop, debugger, collector or compaction
run. Scalar maps remain np0; no managed-reference/calling ABI is enabled.
This is connected production-component evidence, not complete OS/runtime acceptance.
Physical ARM64/RV64 execution remains separate.
