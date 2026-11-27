# INFERNO-0184 — Real arithmetic and numeric conversion continuations

2026-11-14. Extends the existing importing-file client through the normal compiler,
loader, ILOAD/MFRAME/MCALL and scheduler. Production changes are confined to the
shared independent client proof header consumed by compiler and loader. No VM
handler, JIT emitter, Module/Modlink/Frame layout, signature vocabulary, section,
private MP/pool convention or vendor revision changes.

The proof now admits existing ADDF/SUBF/MULF/DIVF/NEGF and CVTLW/CVTFW/CVTLF/CVTFL.
Source and destination kinds are derived independently through one shared numeric
operation classifier. The original MOV/conversion/Word/Big operations use the same
classifier. Arithmetic derives its middle read from either the implicit destination
or an explicit FP operand. Every source/destination/middle role retains exact
kind/span/alignment checks; equal-width Big/Real cannot alias as different kinds.
The CFG checker uses these separate reads/writes for definite assignment before
construction. Real/Big immediates and unauthorized MP/prepared-frame roles remain
refused. The bounded allocation-free analysis and unchanged-output refusal remain.

## New execution and counterexamples

- 18 source pairs/1728 scheduler roots on each x64 and ARM64/Linux QEMU, including
 all four caller/provider C/RX combinations, quanta1/3/1000, stack growth, provider
 open/admission refusal and dontcompile. Every constructor/read allocation failure
 retires. Source image bytes are poisoned before startup. The new cases cover
 arithmetic, negation, compound assignments, positive/negative half rounding,
 Big-to-Word low bits, Word32 minimum, Big-to-Real-to-Big and actual Real/Big file
 provider results feeding numeric continuations and later calls. Runtime transport
 and collector seams in this matrix remain bounded fixtures.
- Both complete consoles rebuild and execute72 new OS roots each: C/RX with actual
 namespace/heap/MP cloning/scheduler/unwinding, plus a physically missing provider
 for each source. Finite out-of-range Real conversions to Int and Big produce the
 existing `invalid argument` and retire normally. Missing files take the source nil
 guard without Broken/panic. Two wrong-result provider controls per target take the
 client's own conditional zero-divide path; default profile selection still refuses.
- 64 independent raw images cover all newly admitted operations and exact source,
 destination, middle, immediate, overlap and initialization roles. Three omission
 controls require semantic exit10 after weakening source kind, result kind or
 definite assignment. No malformed image executes. Host ASan/UBSan and ARM64 gates
 pass; ASan/UBSan constructor-only execution passes all18 source pairs. No new
 whole-VM sanitizer, non-finite arithmetic or FP-environment coverage is claimed.
 The range-error matrix seeds the destination with123 and checks that C/RX
 conversion refusal preserves the entire four/eight-byte destination.
- The32 previous importing Dis64 files (17 linear,15 CFG) remain byte-identical to
 the0183 compiler. All50 classic outputs match that compiler. New18 outputs are
 deterministic and each contains a newly admitted operation; the prior compiler
 refuses all18. The producer explicitly verifies coverage of all nine opcodes.
- Earlier28 raw client inputs/three controls and21 CFG images/four controls pass
 on both targets. The prior32 source pairs/3072 scheduler roots also pass per
 target. The rebuilt complete consoles also pass the previous34 linear
 and60 CFG roots per target with their existing error/retirement controls.
- The complete RU/EN tutorial sources agree, compile in the producer gate and run
 C/RX in both full consoles. The guide explains half-away-from-zero rounding,
 precomputed call arguments, constant-DATA and immutable span-kind limits.

The numeric matrix, boot gate, source coverage and raw omission controls are
integrated into native64-port.py. Reproduce with `--numeric` on the existing
scalar-client.py / scalar-client-boot.py drivers, and the new numeric-proof.py /
numeric-producer.py scripts. Full commands and a working example:
[EN](dis64-client-numeric.md), [RU](dis64-client-numeric.ru.md).

## Next boundary and FlashVM receipt

FLASHVM-0200/09acc38a received and acknowledged in the shared journal. Peer namespace
routing and guarded-return concurrency work stay peer-owned; no implementation is
imported. Vendor pin remains7f2241455734e1142a3254fad9d1ec4f94032ac7.

Ordinary Shell.init is an ordinary exported module function: sh calls it after
load. No automatic loader-init mechanism is proposed. Its context/list arguments,
managed signature matching, frame maps and managed caller operations remain the
next runtime/compiler boundary toward AWM. Linux/QEMU checks here are not evidence
of Android/Bionic GUI execution or a working AWM APK.
