# Native64 local exception recovery: checkpoint 0071

Checkpoint 0070 repaired terminal destruction of prepared frames. This checkpoint
repairs the separate path that catches an exception and resumes its caller.
It changes both owning emu and native-kernel handler implementations. Ordinary
Dis instruction semantics, frame layout and the shared Dis64 file profile stay
unchanged; trusted native calls are still separate from serialized file calls.

## Defects and repair

The handler previously discarded at most one uncalled frame before unwinding.
An argument helper can be active above a prepared outer call, and several
prepared frames can occupy one extent or several extensions. Their arguments
were skipped, or the selected caller resumed with a stale SP/EX image.

The new cleanup validates a complete allocated suffix before changing links or
owners. It links only unentered frames, then releases them from the top down.
SP removes one frame at a time, keeping lower pending owners collector-visible.
Root cleanup uses the existing unextend implementation. At every active caller,
the handler checks for more pending frames/extensions before proceeding or
resuming the selected handler. This includes gaps below argument helpers.

Whole REG snapshots now survive actual argument cleanup, module-owner release,
old exception-slot destruction and pending exception/message cleanup. The
selected caller is published before releasing its departed module owner. The
existing nil-before-old-owner-release and process-to-slot exception transfer
rules remain. Cleanup allocates no new stack/heap storage.

## Owning runtime evidence

`utils/tests/native64-local-handlers.py` uses the production trusted constructor,
FRAME/MOVP/CALL/RET, decoder/xec, heap and stack from checkpoint 0070, plus the
actual emu OR os handler body. Each case raises a real instruction error, catches
it in the original caller, transfers an actual heap String exception and resumes
the VM to IEXIT. No serialized call file is admitted by this fixture.

There are 1344 cases for each port/WORD model, 5376 per target architecture:
C/RX, seven quanta, 80/88-byte callee frames, one/three-extent error states,
three error locations, two ownership routes and four destructor borrowing
profiles. The caller retain is removed only after argument setup; final argument
release therefore executes a real heap destructor during pending-frame or active
helper cleanup. Another real destructor runs when the old exception slot is
cleared. Each callback can overwrite all of REG. Predicates require exact
release counts, the original caller FP/SP/EX/M/MP, selected PC, mirrored Prog.R,
exception transfer, actual resumed IEXIT, balanced types/modules and zero guarded
allocations after final retirement.

Eight pending-owner removal controls restore the old single-frame release body;
eight register controls remove only restoration after old-slot destruction.
Both span emu/os, WORD32/WORD64 and C/RX. They must reach explicit semantic
predicates with statuses10/11; compiler errors, signals or sanitizer failures
are not accepted. The complete native64 port driver runs these gates on native
ARM64/x64 and RV64/QEMU.

OS allocation, process lookup and diagnostic formatting are bounded adapters.
The trusted fixture has no concurrent collector or OS scheduler. Separately,
the existing exception/GC gate executes real rootset/markheap/rungc epochs at
publication callbacks. The old exception-frame matrix retains all alignments,
metadata refusals and ownership cases. Its synthetic caller padding is now
included in the caller Type size: an untyped hole between frames is not a valid
allocated pending-frame record. Addresses and alignment coverage are retained.
This is not a full native-kernel boot, parallel scheduler or graphical startup.

## Shared Dis64 boundary

FLASHVM-0061 explicitly accepts the future eight-byte LEA result-address rule,
independent of numeric WORD width, restricted initially to the prepared callee's
complete aligned byte32 cell and a live typed caller result span. The peer has
recorded that narrow agreement in `doc/dis64-contract.md`. This checkpoint
retains it and allocates no section/opcode/feature ID. Calling metadata,
signature/verifier admission, managed results and executable file fixtures
remain separate work. Current readers continue to refuse file LEA/CALL.
