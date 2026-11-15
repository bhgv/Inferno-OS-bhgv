# INFERNO-0193 — native-width array element addresses

2026-11-15. Base426f4333 /0192. The C runtime and AMD64/ARM64/RV64 emitters now
return full native element pointers from INDB, INDW, INDL, INDF and INDX, including
Word32/ref64. Indexes and array lengths remain scalar WORD values. LENA remains
a scalar WORD result. This closes the runtime narrowing found in0192; selected
Limbo file admission for element indexing is still deliberately closed.

## Contract correction

Previously arrayindex converted arrayaddress to WORD and wrote sizeof(WORD).
A valid address above4GiB therefore lost its high half in Word32/ref64. Emitters
made the same truncation, so their old modulo-WORD comparison oracles could agree
with C while both returned unusable native addresses.

The C helper now captures a pointer and copies sizeof(pointer) into R.m, after
checking that full result span. Scalar index reads still use sizeof(WORD).
AMD64 and ARM64 preflight eight result bytes and store eight bytes; their scalar
index/length guards are unchanged. RV64 publishes both halves through its
existing split-word store helper, retaining four-byte operand-slot alignment.
Fixed strides remain1/WORD/8/8; INDX keeps descriptor-sized and zero-width rules.
Bounds, carry, lazy metadata checks, failure precedence and captured-source alias
semantics remain intact. No allocation, GC or frame/type layout is changed.

This intentionally changes the borrowed result-slot contract for the experimental
Word32/native-pointer64 model. Such callers need a full native pointer slot.
Ordinary32-bit pointer/WORD targets retain four-byte C results; existing native
WORD64 results remain eight bytes. No public Dis64 section/kind/signature is
assigned or changed. Existing selected images do not contain these instructions.

## Executed evidence

- Independent `array-pointer-width.py` uses all three production emitters with
 storage above4GiB:13824 image/canary/alias cases per backend,41472 total. Both
 scalar WORD widths, five opcodes, generic strides0/1/3/8/16, source/index/output
 offsets0/1/4/7 and output aliases are checked against an eight-byte pointer
 oracle. Reverting each backend to a WORD-sized store is detected semantically.
 These omission controls are mandatory in the default driver.
- Updated array-slot C/RX differential gate passes547232 WORD32 and548000 WORD64
 cases on native Linux x64, and the same cases on ARM64/Linux QEMU. Full output
 spans, inaccessible operands, header wrap/lazy reads, aliases, exact canaries
 and preexisting FP exception flags are checked. Host ASan/UBSan passes both.
 The driver now accepts NATIVE64_ARCH/CC/NATIVE64_RUNNER for ARM64 reproduction.
- Independent AMD64/ARM64 Unicorn slot/header/alias matrix passes22066 cases per
 backend. Complete image effects and write-free declines are checked. The general
 leaf array/descriptor groups pass53170 and28066 checks across all three emitters.
 RV32/RV64 feature-regression matrix passes2174 checks; coverage counts are
 unchanged. RV64 is instruction emulation/publication evidence, not physical
 RV64 or a full RV64 VM run. Unaligned RV emulation cases are not a new hardware
 alignment guarantee.
- Actual C declaration/handler oracles pass10247040 WORD64 +10239360 WORD32
 array-index checks and85248 descriptor-index cases per WORD model under host
 ASan/UBSan. Replaying the parent C on valid WORD32 cases fails the full-pointer
 oracle without a sanitizer diagnostic. Old narrowed results are not masked away.
- Complete AMD64 leaf compiler/runtime gates pass both WORD models: actual RX
 entry calls, fallback, publication failure/retry, descriptor operations and unmap.
 ARM64/RV64 compiler publication/metadata/failure gates pass both models on the
 host without executing those foreign compiler entries. Actual ARM64 array RX
 execution is the separate QEMU differential gate above.
- Rebuilt complete consoles pass572 existing main Command C/RX roots each on
 x64/ARM64 Linux QEMU, policy/hash/entryless controls and the unchanged tutorial.
 No source producer, serializer or selected opcode admission changes in this
 batch. The new independent test is in slice-tokenize-suite.py's JIT list.
 Full suite, full-VM ASan/concurrent GC, Android/Bionic GUI/device/APK and
 physical ARM64/RV64 acceptance are not claimed.

Reproduce with array-indices.py, descriptor-index.py, array-slot-native.py,
array-slot-jit.py, array-pointer-width.py, leaf64-jit.py (array/descriptor groups)
and leaf64-runtime.py in libinterp/tests. Existing requirements-jit.txt supplies
instruction-emulation dependencies; no runtime dependency is introduced.

## Next compiler boundary and peer receipt

The selected source compiler still represents an array-address temporary as an
int declaration in the legacy IR. For example, a classic assembly listing uses
`indb ...,68(fp),...` followed by `cvtbw 0(68(fp)),...`; the native typed planner
cannot simply leave that temporary four bytes wide after this runtime fix.
It needs an unmapped, native-width borrowed-address slot and an owner/consumer
proof across instruction quanta. Byte constants in MP also need trusted lowering;
selected globals presently admit String literals, not arbitrary byte DATA.
Enabling INDB without those checks would be incorrect.0192's raw refusal and
current file-profile restrictions remain unchanged. General indexed arrays and
AWM/Android source execution do not follow from these low-level gates.

Reviewed FlashVM master
[c9dc3b4c](https://github.com/bhgv/flashVM/commit/c9dc3b4c075fa4aa697957094e0a74adc4317935);
its arrayindex still contains the old WORD cast. Sent this corrected C/JIT result
contract and exact evidence through the own Inferno journal. No DisVM migration
or peer code import; vendor pin7f224145 is unchanged. Incoming journal remains
FLASHVM-0202/0203. Peer callers/oracles must use native pointer result storage
when importing this correction; a scalar WORD32 result slot is insufficient.
