# 64-bit JIT opcode availability — 2026-11-01

Measurements invoke actual bounded production emitters. The denominator is 173
active Dis opcodes:175 enum slots minus invalid INOP and unused IECLR. A guarded
native path means only supported operand cases; declined cases use C. These
percentages do not measure runtime input coverage, speed or complete Dis64 ABI.

| Backend/profile | Native fast paths | Availability | No native fast path |
|---|---:|---:|---:|
| AMD64 |166/173|95.95%|7|
| ARM64 |166/173|95.95%|7|
| RV64I |87/173|50.29%|86|
| RV64I + Zmmul |90/173|52.02%|83|
| RV64M |103/173|59.54%|70|

Counts agree for the tested WORD32 and legacy WORD64 layouts. Actual emitted code
is selected according to the target multiply/divide feature flags; availability
must not be inferred from a host build that lacks those target flags.

AMD64, ARM64 and RV64 now have173/173 callable complete JIT tail entries,
including C-only operations. RV64 complete tails use RV64I instructions and work
with I/Zmmul/M profiles; native fast availability above is unchanged. The original
decoded VM loop still prefers a direct fast entry when present and uses the
complete tail for C-only operations. A declined direct fast entry keeps ordinary
C fallback. Allocation, scheduling, linking and ownership-heavy work remain C.

RV64 compiler/cache gates verify complete tables in WORD32/WORD64, I/Zmmul/M and
embedded/RW-to-RX page configurations. New Linux/QEMU target tests execute the
actual bridge and C executor for all 173 opcodes, both four/eight-byte code-entry
alignments, addresses above 4 GiB, native acceptance/decline/C-only routes and code
retirement inside a C callback. Tests cover RV64I/RV64IM/RV64GC and both WORD
layouts, with minimal target process/REG/handler services rather than a full VM.
See [RV64 complete-tail evidence](../libinterp/tests/README.riscv-dispatch.md).

The new Dis64 loader accepts a43-op scalar subset. Its connected instruction
fixture runs all43 through actual C/AMD64 RX decoder/VM paths. The scheduler
fixture separately exercises MOVW, ADDW, DIVW, SHLW and IEXIT with actual root and
terminal process ownership. This does not enable the remaining130 opcodes in
that new-format loader, nor references/calls/GC, nor a default format switch.
An explicit DIS64_PILOT_LOADER=1 build now connects the file reader/parser to
the same scalar constructor; see [file-loader evidence](dis64-fileloader.md).
The [vmachine gate](dis64-vmachine.md) now joins that route to actual
handler/propex/error selection, with C and AMD64 RX dispatch; legacy table
injection is separate from the accepted Dis64 scalar wire profile.

Reproduce from the repository root:

```
python3 libinterp/tests/jit-coverage.py --json
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/dis64-runtime.py
python3 libinterp/tests/dis64-scheduler.py
python3 libinterp/tests/dis64-vmachine.py
```

Native execution in the connected Dis64 fixtures is Linux/AMD64. RV64 complete tails additionally run under Linux/QEMU; this does not replace a
physical target, full scheduler/GC startup or complete new-format admission.
ARM64 availability and host compilation likewise do not establish target execution.
