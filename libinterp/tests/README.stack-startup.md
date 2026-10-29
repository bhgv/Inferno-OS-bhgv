# Stack extents, process startup and frame indices

Run `python3 libinterp/tests/stack-startup.py` from the repository root.

The fixture executes both emulator and native-kernel scheduler bodies through
real emulator declarations and a native incref adapter. The current default matrix runs
2,614 native-WORD cases and 2,602 word32 cases per scheduler, including seeded
PID exhaustion (10,432 cases total). Independent exception families run in fresh
processes. Pointers keep host width; word32 is not a complete
32-bit ABI build.

Production bodies include newstack/extend/unextend, frame/mframe/spawn/mspawn,
metadata resolution, newprog/newgrp/schedmod, initialization of mapped pointers,
and both protection/rethrow implementations. Tests cover:

- Extent geometry for six frame sizes and eight stack hints, minimum/exact fits,
  real native header offsets, signed growth boundaries, oversized hints and
  unchanged source bytes on success and failure.
- Full-sized initial frames with zero scalar arguments and H pointer arguments;
  failure at every root-startup allocation, including group creation; module
  owner balance, scratch-register restoration and empty scheduler/PID tables.
- Child stack and Prog allocation failures before publication, preserved source
  arguments, correct target PCs, environment failures before/after user-string
  assignment, and parent cancellation while copying the environment.
- Full-WORD descriptor and link indices, positive imports, negative exports,
  missing sentinel entries and high 32-bit values on the native WORD model.
- PID exhaustion detected before signed increment. The driver seeds only the
  private counter and maps the expected terminal panic to a fixture error.

A bounded allocator guards each exact payload. Scheduler insertion, PID lookup,
FPU operations, parent-group attachment, final teardown, user copying and
linkmod are injected boundaries. User copying also injects errors/cancellation;
linkmod follows the separately tested module-owner contract. unframe records
successful argument transfer rather than unwinding a real parent VM stack.
No child arguments are destroyed when an unpublished stack copy is abandoned.
Actual nested MP copying remains covered by heap-clones.py.

Strict UBSan is the default. ASan passes with stack instrumentation enabled:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/stack-startup.py
```

Only unsupported LeakSanitizer process inspection is disabled.
`STACK_SOURCE`, `STACK_XEC_SOURCE` and `STACK_DIS_SOURCE` select alternate
production files. `STACK_CASES=stack|root|spawn|indices|environment|overflow|pid`
isolates failure families. Original bodies independently reproduce extent
underallocation, a root stack-buffer-overread, premature child publication,
unvalidated indices, environment-error leaks, and signed stack/PID overflow.
The existing fp-spawn.py still verifies destructive FPU-save restoration and
inheritance on both scheduler variants.

This does not boot Inferno, validate complete OS process/group lifetime, execute
native compiled frames, run concurrent scheduling or GC/compaction, or activate
Dis64. Real native-kernel compilation and generated-JIT stack layout remain
integration checks; syntax checks use Linux/Inferno ARM/386 headers and compile
the full emulator scheduler.

Module-spawn rejection also covers native NULL and VM nil instances, plus
instances with either missing owner representation. These cases preserve VM
registers, allocate no child stack and publish no process. The driver runs
these checks for emulator/kernel variants with native and 32-bit VM words.

`STACK_CASES=framealignment` adds 2,048 cases per scheduler/word combination:
independent byte offsets for the descriptor/module source, link index and pointer
destination, on both the inline and extend/unextend paths. All four combinations
pass ASan/UBSan (8,192 new cases). The complete default suite passes UBSan.
The complete ASan run is not claimed: the pre-existing root-startup family reports
a stack diagnostic at krethrow after siglongjmp, alongside the environment's
missing stack-top warning. `STACK_CASES=root` reproduces the same diagnostic with
parent 77a1a89 xec.c; stack instrumentation has not been disabled to hide it.

Root startup now uses include/vmstartup.h before cloning MP or creating a Prog.
Missing, one-past and misaligned relative bytecode entry PCs, invalid program
counts and unusable frame metadata are rejected before publication. Existing
opaque native-PC entries remain supported. schedmod rejects nil/invalid owner
representations without trying to consume a nonexistent Module reference.
The root fixture now supplies the real program span required by this contract.
Actual Dis64 scheduling and terminal cleanup have a separate connected driver:
`python3 libinterp/tests/dis64-scheduler.py`; see doc/dis64-scheduler.md.
