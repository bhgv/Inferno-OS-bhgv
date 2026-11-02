# Native64 local calls: checkpoint 0070

This gate exercises trusted decoded native instructions, not a new Dis64 file
calling ABI. The shared scalar file profile remains 43 operations with np0 maps.
No opcode, feature, section or signature ID is allocated here.

## Defect and repair

`FRAME` allocates storage and initializes mapped argument cells before `CALL`
links the frame into the active FP chain. A failing argument expression can
therefore leave reference-owning frames outside that chain. An argument helper
can itself be active above an already prepared outer call. Previously terminal
`destroystack` followed only the active chain: pending arguments leaked, and an
unentered extension could apply its root descriptor to the caller's frame.

The destructor now uses the captured SP/TS of each extent to recover the complete
allocated gap above each surviving caller. It validates the entire gap before
linking pending frames in reverse physical order for ordinary mapped cleanup.
This covers pending suffixes, gaps below active argument helpers and unentered
extensions. Parent extent links, bounds and fallback root descriptors are captured
before callbacks. No allocation, register-layout change or JIT opcode replacement
is needed. Snapshots without coherent SP/TS keep the existing chain-only path.
This repair concerns terminal stack destruction; it does not establish recovery
and resumption at arbitrary language exception handlers.

## Execution checks

`utils/tests/native64-local-calls.py` links unchanged production FRAME, MOVP,
CALL, RET, decoder, xec, heap and stack bodies with the owning native JIT backend.
Only allocation/OS/collector boundaries are supplied by the existing bounded
runtime fixture. There is no concurrent collector, OS scheduler or file loader
in this test.

The 896 cases (448 each in WORD32 and WORD64) cover C and RX dispatch,
root-only and three-extent states,
80/88-byte managed callee frames, quanta 1/2/3/7/16/64/1000, global and saved REG
cleanup, success, divide-by-zero before CALL, divide-by-zero inside an argument
helper and a nil CALL target. Two calls share a real heap owner via actual MOVP.
Checks require the expected returned value or exact error, exact reference and
extent counts, balanced module ownership, intact allocation guards and zero live
allocations after complete teardown. The existing descriptor reader must refuse
an 84-byte mapped extent; no new alignment policy is inferred from that refusal.

Four removal controls disable pending-frame recovery in C and RX in both
WORD models, without growth,
and must reach the explicit leaked-owner predicate with status 10. Compilation,
crashes or sanitizer failures do not count as successful controls.

The port driver runs both gates on native ARM64/x64 and RV64/QEMU. Native CI
results are recorded in the outgoing coordination journal after publication.

## Remaining file-call boundary

The trusted constructor binds a full-width native result address into a private
root cell and transfers it with MOVL. These pointer bytes are never read from a
Dis64 file. Ordinary LEA writes an active WORD-sized address: with DIS_WORD32
that is four bytes, so it cannot establish this 64-bit result binding. Source
inlining avoids that binding and remains the only admitted file path for these
helper functions. A shared typed frame/result/address contract must be agreed
with FlashVM before general file FRAME/CALL admission. This gate is evidence for
runtime mechanisms and cleanup, not evidence that this remaining boundary has
been opened.
