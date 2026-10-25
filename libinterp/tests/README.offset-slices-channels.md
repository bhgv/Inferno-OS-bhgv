# Nonzero byte-slice indexes and channel lifetime checkpoint

AMD64 and ARM64 ISLICELA leaves now accept arbitrary valid destination indexes
for non-pointer byte elements and source lengths up to 128. The original
64-unit private emission limit is unchanged. This expands an existing opcode's
operand coverage; availability remains 128 native paths / 45 interpreter-only
commands for both backends out of 173 active commands. RV64 stays at 103/70
with M, 90/83 with Zmmul and 87/86 with the base integer ISA. INOP and unused
IECLR remain outside the active count. WORD32/native-pointer64 and legacy
WORD64 have the same counts.

## Geometry and generated code

The destination must have a nonnegative length. Unsigned index bounds against
that signed, nonnegative length reject negative indexes; subtraction then bounds
the source length without overflowing. For a nonempty copy, index + length must
fit INT_MAX, matching the complete arrayspan element limit. Checking only the
index would incorrectly accept some legacy-WORD64 spans crossing that boundary.
The earlier bounds also ensure this sum cannot overflow native unsigned math.

Destination-base + index, source-base + count and destination-start + count all
have unsigned address-wrap checks. For a positive count, these checks reject H
as well as wrap, allowing redundant H comparisons to be removed. C-null bases
are still rejected before adding an offset. Empty copies return before element
geometry, type or data validation, preserving C behavior for wide empty indexes.
Absent sources retain their earlier no-op behavior.

Both overlap directions retain a signed-stride byte loop. ARM64 reuses one
count-minus-one value to set up the reverse path. AMD64 combines the null/H
source tests and narrows only the already-bounded loop counter, preserving its
full-WORD metadata checks. No helper calls, stack writes, ownership changes or
FP-state changes were introduced. Large or mapped copies retain optimized C.

## Channel corrections

- cgetb rejects a C-null buffer and validates a nonzero, nonmaximum buffer
  reference count before its temporary retain, ring-position change or move.
- Buffered irecv validates required channel/buffer temporary counts before
  removing or selecting a queued sender. Failure leaves ring bytes, queue nodes,
  peer state, decoded operands and scheduler state unchanged, without leaking a
  channel retention before a later buffer-count error.
- cnewc rejects H as well as C null when capacity or the mapped mover requires
  a Type descriptor. Failure occurs before any heap/queue allocation or Type
  counter change. An unbuffered scalar mover still permits an unused descriptor.

The isolated queue fixture now defines the exHeap symbol used by the real
receive handler's new error paths.

## Validation

- slice-copy-native.py: 4792416 cases per WORD model, 9584832 total, execute
  actual AMD64 RX leaves and production arrayspan/slicela against independent
  complete byte images and libc memmove under ASan/UBSan. The matrix covers
  every source/index/destination alignment, indexes zero through seven,
  overlap, operand aliases, element sizes zero/one/two and zero-bit-map fallback
  traversal. Explicit cases execute the exact INT_MAX endpoint, reject spans
  crossing it, preserve wide empty indexes and absent-source behavior, and
  compare exact VM error identities. Generated code itself is not instrumented.
- slice-copy-jit.py: 203322 combined AMD64/ARM64 execution/metadata/span/alias/FP
  checks with production bytes, Capstone and Unicorn. New cases include
  nonzero offsets, simulated valid high-offset data spans, geometry overflow,
  offset/address wrap and wide empty indexes. Full memory images above 4 GiB
  and guest write hooks verify zero/declined invocations perform no stores.
- mapped-copy-layouts.py: 106080 probes per architecture, 212160 total, pass
  ASan/UBSan for all three recent copy opcodes, large native layouts and every
  capacity from zero through 64, preserving output canaries on rejection.
- CHANNEL_TRANSFER_CASE=retains adds 1024 cases per port/model, 4096 total,
  under ASan/UBSan. It varies null/zero/MAX temporary owners, queued senders,
  scalar/pointer buffers and all source/destination alignments. Actual C queues,
  callbacks and both emu/os error protectors remain in use; complete channel,
  ring, peer, register and queue images must remain unchanged on failure.
- CHANNEL_REQUIRED_TYPES=1 passes six descriptor/capacity/no-allocation checks
  per WORD model, twelve total, under ASan/UBSan. Selecting the parent source
  reproduces the H descriptor fault. Parent receive tests reproduce buffer and
  channel count wrap before destruction using CHANNEL_RETAIN_KIND=1/4.
- The complete core suite passes 71 host drivers after the queue fixture update.
  The standalone queue regression also passes 218260 scenarios. Actual AMD64
  publication/execution passes ASan/UBSan for both WORD models, ARM64 publication
  passes both models, and RV64 publication is unchanged.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/slice-copy-native.py
PYTHONPATH=../jit-test-deps python3 libinterp/tests/slice-copy-jit.py
python3 libinterp/tests/mapped-copy-layouts.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' CHANNEL_TRANSFER_CASE=retains python3 libinterp/tests/channel-transfers.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' CHANNEL_REQUIRED_TYPES=1 python3 libinterp/tests/channels.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

XEC_SOURCE selects alternate channel receive implementations;
CHANNEL_CREATE_SOURCE selects alternate constructors. CHANNEL_RETAIN_KIND and
CHANNEL_RETAIN_QUEUED isolate receive failures. Physical ARM64 execution and
full VM/native-block/frame/GC ABI acceptance remain separate. Tk, security/
crypto and MIPS/SPARC/PowerPC JIT sources are outside this checkpoint.
