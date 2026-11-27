# Native64 fresh exception messages: checkpoint 0072

Checkpoint 0071 catches a real instruction error and transfers an existing heap
String into the recovered caller. Ordinary interpreter errors also take another
path: no process-owned exception exists, so the handler must construct a new
message after unwinding. This checkpoint exercises that path and fixes its
process-root publication boundary in both emu and native-kernel handlers.

## Root publication repair

After prepared frames and active helpers have been discarded, global R already
names the surviving caller. Previously Prog.R was copied only after message
construction and final slot publication. Until then its SP/EX/FP/module image
could still describe discarded frames or freed extensions. Production rootset
reads Prog.R, not an arbitrary native C-stack REG snapshot.

The handler now publishes the complete surviving REG before cleanup and message
allocation. The final publication after selecting the handler PC remains. This
is a serialized ownership boundary, not an atomic concurrent-mutator or collector
barrier. No frame layout, instruction semantic, wire tag or file admission changes.

## Actual constructor and root-scanner closure

`utils/tests/native64-fresh-handlers.py` specializes the preceding real-call
fixture. It links unchanged production excinit/setstr/newestring, c2string,
newstring/newrunes/hmsize, UTF decoding, rootset and recursive markheap, in
addition to the real heap, frames, decoder/xec, C/RX and emu/os handlers.
The pool adapter supplies exact tracked extents, immutable emergency-buffer
identity and a controlled pre-allocation observation. There are no external
pointer roots in this fixture.

At the message allocation boundary, predicates first require FP/SP/TS/EX/M/MP
and the full process REG mirror to describe the surviving caller. This avoids
turning the old stale-root defect into a sanitizer-only use-after-free test.
Actual rootset/markheap then have to find a two-object live graph through the
caller's declared map. Its native observer pointers are deliberately not
registered as roots. Removing the map bit must leave that graph unmarked.
The previous exception slot already owns nil when allocation begins.

The full matrix has 21504 cases per architecture: 5376 for EACH emu/os and
WORD32/WORD64 selection. It retains actual C/RX errors, seven quanta, two frame
sizes, growth, three instruction-error locations, two ownership routes and four
destructor borrowing profiles. Each case constructs either the exact ASCII
message or independently checked U+0436 from UTF-8, with either normal allocation
or an injected primary allocation failure. The latter runs the actual protected
error transfer and retains the real emergency buffer from excinit; its byte
payload and two-owner reference count are checked. Constructor allocation may
also borrow all of global REG; resumed state must survive that callback.

Each case resumes the VM to IEXIT and requires exact messages, balanced protected
error depth, references/types/modules, guarded allocation extents and zero final
allocations, including retirement of the fixture's emergency-buffer owner.
Eight publication controls remove only the early Prog.R copy; eight map controls
remove only the caller graph's map bit. They must reach explicit statuses12/13,
not a compiler error, signal or sanitizer failure. The native64 port driver runs
all positive and control gates on native ARM64/x64 and RV64/QEMU.

This closure performs actual root enumeration and recursive marking, but does
NOT run a sweep, concurrent collector, OS scheduler or native-kernel boot. The
existing separate exception/GC fixture retains its full collector-epoch tests.
The pre-existing transfer-only and terminal-call matrices remain independent
regressions. No Android graphical-startup or parallel managed-spawn claim follows.

## Shared Dis64 work

The narrow future LEA/address agreement from INFERNO-0070 / FLASHVM-0061 is
retained. Publishing a live caller/root snapshot before allocating is also a
prerequisite for future managed results. It does not establish function
metadata, signatures, safepoint coverage, executable file-call admission or
new extension IDs. The current shared 43-op np0 profile remains unchanged.
