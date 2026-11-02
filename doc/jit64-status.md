# 64-bit JIT opcode availability — 2026-11-02

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

Connected scalar Dis64 fixtures execute on Linux/AMD64 and Linux/ARM64 under
QEMU. The ARM64 gate covers744 whole images and1667800 actual C/RX predicates,
plus actual scheduler/file/vmachine closures and exception-removal controls.
Complete classic-mk ARM64 and x64 console emulators also boot the two scalar
pilots with JIT off/on and trusted Math/Sys registration. The lease/GC experiments
remain private C fixtures; this does not establish a file-native calling ABI.
RV64 complete tails additionally execute under Linux/QEMU. Subsequent
native64-port runs through checkpoint0073 also build and execute the complete
classic-mk RV64 scalar console under QEMU, alongside native ARM64/x64; see
[the coordination/validation journal](dis64-inferno-journal.md). This remains
restricted scalar console evidence, not general application boot. QEMU evidence
does not replace physical hardware,
full shell startup, general managed call graphs or complete new-format admission.

The clean build/boot/lease reproduction driver is utils/tests/native64-port.py;
see [native64 build instructions](build-native64.md#clean-native64-build-and-execution-reproduction).
Fast opcode counts above are unchanged by this build series. Complete JIT+C
entry coverage remains173/173; the shared scalar file profile admits43/173
opcodes (24.86%), and does not admit the remaining130 or file CALL/MCALL.

The guarded fixed scalar experiment now has an explicit live-R C adapter, with
publication before acquisition/allocation and owning interpreter return/error
retirement. Actual x64 and ARM64/QEMU vmachine and live-GC gates cover that
private contract. It does not open file CALL/MCALL or change opcode counts.

The private fixed scalar adapter additionally accepts compiler-produced Real
field witnesses tied to exact caller Module/Type/callable identity and the held
activation generation. Its C producer checks execute on x64 and ARM64; standalone
field and sequence checks also execute on RV64/QEMU. These are calling-contract
checks, not new native opcode paths or expanded Dis64 file admission.

Series0055 adds fault-PC/source correspondence and optimized UTF semantic checks,
not opcode emitters. The complete classic-mk x64, ARM64 and RV64 scalar consoles
retain their existing interpreter/JIT entry coverage. Source locations remain
private offline tooling; the shared file profile still admits43/173 instructions.
The UTF fixture's prior NDEBUG-disabled assertions are replaced by active `if`
predicates plus wrong-count executable controls. This does not close the earlier
ASan error-unwind report or broaden managed/native file-call admission.

Native Linux ARM64 execution is now confirmed in CI run41334049753, including
the complete classic-mk scalar console and744-image C/RX/ownership gates. The
same run passes native x64 and QEMU RV64 and preserves downloadable console
artifacts. Full optimized UTF ASan+UBSan additionally passes on native ARM64/x64
in run41334449491. These results strengthen platform/test evidence; they do not
change the opcode counts,43-op file profile or open full managed application
startup. See [series0055](native64-series0055.md#native-linux-arm64-ci-acceptance).

Checkpoint0074 adds private abstract-control preflight, without changing any
opcode count above. FLASHVM-0063 accepts the initial export catalog, final
eight-byte caller result cell and single-prepared-frame/acyclic-call subset
design. The private catalog and control checkers do not enable file calls.
The remaining admission work is decoded operand/setup-state verification,
definite assignment and signature-kind/result-lifetime checks, then whole-file
C/RX positive/refusal goldens on the three target architectures. See
[control preflight](dis64-local-flow.md) and
[accepted initial design](dis64-local-call-proposal.md). This is a Linux console
checkpoint; it does not produce or validate an Android APK or GUI.

Checkpoint0075 adds private executable preparation/result state transitions and
strict live-state joins, with98032 independent oracle records and13 semantic
removal controls. It consumes trusted abstract facts: decoded operand roles,
source-value/span proofs, entry-seeded CFG propagation and composed callee return
guarantees remain before file calls. This changes no fast-path or admission
count. See [state scope](dis64-local-state.md) and
[series0075](native64-series0075.md).

Checkpoint0076 composes the private state model with entry-seeded semantic CFG
propagation:4005 graphs and eight active semantic controls. Scratch is reset;
exact joins, setup ownership, all-node static preflight and work/capacity limits
are checked. This advances abstract history proof but still does not derive
actual decoded operand/source/span facts or enable file calls. Fast paths and
43-op file admission are unchanged. See [CFG scope](dis64-local-cfg.md) and
[series0076](native64-series0076.md).

Checkpoint0077 repairs scalar helper width validation and pre-narrowing memory
bounds, and derives typed numeric read/write facts from real compact code bytes.
The17544-case byte/layout oracle and12 controls remain private fact-extraction
evidence under trusted declarations. Numeric initialization and callable/CFG
composition still precede new file calls. No fast-path or43-op admission count
changes. See [operand scope](dis64-local-operands.md) and
[series0077](native64-series0077.md).

Checkpoint0078 adds actual scalar-code numeric definite initialization: canonical
argument entry seeds, read-before-write checks, intersecting CFG joins and bounded
requeue convergence. The2029-case synchronous oracle/13 controls compare every
reached set. Context provenance/callable return composition remains before new
file calls; no native fast-path or43-op admission count changes. See
[numeric scope](dis64-local-initialized.md) and [series0078](native64-series0078.md).
