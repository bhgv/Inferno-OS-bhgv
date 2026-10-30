# 64-bit JIT opcode availability — 2026-10-30

Measurements invoke actual bounded production emitters. The denominator is173
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

AMD64 and ARM64 also have173/173 complete JIT tail entries, which retain complex
operations in C. RV64 currently uses the original VM C dispatcher for missing or
declined leaves; it does not yet publish the same complete tail-entry table.
Keeping an operation in C is a supported design choice. Allocation, scheduling,
linking and ownership-heavy work need not be duplicated in assembly.

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

Native execution in the connected Dis64 fixtures is Linux/AMD64. ARM64/RV64
emitter availability and host compilation do not replace physical target runs.
