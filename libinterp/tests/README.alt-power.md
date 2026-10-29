# ALT slots, partial cleanup and negative integer powers

Subsequent [ALT publication checks](README.alt-publication.md) cancel registrations
before result/index/payload aliases can overwrite guard metadata.

This series repairs the shared ALT implementation rather than changing the
public Alt/Altc or channel ABI. It covers preparation, selection, wakeup and
cancellation, plus the native send helper and string-to-big VM conversion.

* Capture Alt/Altc fields and channel-array entries with byte-safe copies.
  Frames, arrays of channel pointers, selection results and receive tuples may
  have different alignments. Selection words and tuple indices use `memmove`.
* Validate signed guard counts and their complete table span before registration;
  reject negative/oversized channel-array lengths, missing nonempty backing data,
  and native address wrap before walking members. Empty-array identities remain
  valid and do not require backing data.
* Reject C-null scalar/member channels without dereferencing them. Cleanup skips
  invalid members and invalid array spans while cancelling earlier registrations.
  Foreign queue members and their FIFO order remain intact.
* Check additions to the count of ready channels before signed overflow.
* Reject both H and C-null native send owners before changing REG or consuming
  a payload. `cvtcl` captures its source owner and publishes an unaligned LONG.

The x86-64, arm64 and RV64 integer-power leaves now handle negative exponents,
including the minimum signed VM exponent, without negating it. Base 1 yields 1;
base -1 uses parity; every other nonzero base yields 0. Zero to a negative power
still declines without writing the destination so the C VM raises `exZdiv`.
All input values are captured before a possibly aliased result is stored. Positive
modular powers retain their existing algorithm. RV64 requires multiplication
(M or Zmmul), and this path requires no division or FP instructions.

## Validation

```
QUEUE_CASE=altslots python libinterp/tests/channel-queues.py
QUEUE_CASE=altgeometry python libinterp/tests/channel-queues.py
CHANNEL_TRANSFER_CASE=ready python libinterp/tests/channel-transfers.py
CHANNEL_TRANSFER_CASE=wake python libinterp/tests/channel-transfers.py
CHANNEL_TRANSFER_CASE=faults python libinterp/tests/channel-transfers.py
python libinterp/tests/string-slots.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=power python libinterp/tests/leaf64-jit.py
python libinterp/tests/leaf64-runtime.py
LEAF_WORD_BITS=32 python libinterp/tests/leaf64-runtime.py
python libinterp/tests/slice-tokenize-suite.py --core
```

All C fixtures extract production functions. The queue and transfer fixtures run
emu and os scheduler/error helpers in both WORD64 and WORD32 models with native
host pointers. These are not full 32-bit-pointer or boot tests.

* ALT selection/cancellation alignment: 24,576 cases per scheduler/model,
  98,304 total, with independently aligned headers, channel-array backing,
  tuple and result, and foreign waiters.
* Actual ready transfers: 6,144 cases per scheduler/model, 24,576 total.
  Actual blocked wakeups: 3,072 per scheduler/model, 12,288 total. Scalar and
  pointer payloads, buffered and rendezvous channels, scalar and array guards
  use byte-safe independent fixture reads as well.
* Malformed ALT geometry: 12 cases per scheduler/model, covering partial queue
  rollback, invalid headers/owners, negative/large arrays and address wrap.
* String slot matrix: 79,111 cases per word model, including ASCII, runes and H
  for word, real and big parsing, owner aliases and surrounding-byte guards.
* Three-backend power emulation/decoding: 12,316 checks including WORD32/WORD64,
  minimum/large exponents, signed bases, aliases and exception fallback before
  an inaccessible destination. The native AMD64 lifetime/runtime fixture also
  asserts direct leaf acceptance for every supported power case before checking
  the C/fallback dispatch oracle.

Parent overrides `ALT_SOURCE` and `STRING_SLOT_XEC_SOURCE` reproduce unaligned
pointer loads/stores and a null member dereference. See the driver environment
variables for selecting families independently under ASan/UBSan.

Real-to-integer conversions remain in C. Producing the right integer alone
is insufficient: C's final float-to-integer conversion can set FE_INEXACT even
for an integral source after adding the rounding half. Future native versions
must preserve the target FP rounding, status and trap behavior as well as the
integer result. GP-only reconstruction would lose those observable effects.


## ALT header/result spans and communication error cleanup

ALT/NBALT now reject a wrapping fixed header before copying either signed count.
The selection result must hold a complete WORD before any queue registration or
scheduler transition. `altstore` checks the same output geometry when publishing
asynchronous selections. Byte-aligned result cells and guard tables remain valid.

The selected communication runs inside a real VM protected boundary. If it raises,
all registrations belonging to the current alt are cancelled before rethrowing
the original error. Scalar guards, channel arrays and duplicate registrations
preserve unrelated queue members and FIFO order. This does not roll back payload
operations, peer wakeups or selection/index stores that already happened before
a transfer error; it closes the registration leak.

The queue driver supports `QUEUE_JIT_ROUTE=1`: it compiles actual complete ALT
and NBALT tails for the host backend, links production `cjitexec` and the real C
handlers, and executes the same queue matrices through those published RX entries.
Both entries deliberately have no native fast leaf. The run here uses AMD64;
the driver selects ARM64 on an ARM64 host, whose physical run is still pending.

Validation with ASan/UBSan and NDEBUG:

```sh
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
QUEUE_JIT_ROUTE=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-transfers.py
```

The direct and complete-JIT queue matrices pass 243,860 cases each across emu/os
and WORD32/WORD64. The real payload transfer matrix passes 599,408 cases, including
buffer FIFO/rings, retained references, ready ALT transfers and blocked wakeups.
These are native-pointer models, not a completed native VM ABI.

The added span family passes 288 cases per WORD64 runtime and 224 per WORD32
runtime, per route. The transfer-error family adds 6,144 cases per runtime/model
and route: both guard kinds, eight table/result shifts, duplicate registrations,
foreign waiters and queued senders. The communication error is raised before the
foreign sender is removed; the fixture proves its payload and queue survive.
Together these new families account for 51,200 C/RX cases.

Six independent preceding-source controls fail: header, result and communication
families through direct C and complete JIT. The old header reaches SIGSEGV, the
old result reaches UBSan's nil-output diagnostic, and the old communication path
fails the active survivor-queue predicate. `ALT_SOURCE`, `QUEUE_CASE=altspans`,
`ALT_SPAN_KIND=header|result`, `QUEUE_CASE=alttransfererrors` and
`QUEUE_JIT_ROUTE=0|1` select the controls.

Both fixtures now use explicit predicates, retaining all inherited checks under
NDEBUG. The queue-only fixture explicitly rejects any unexpected buffered-owner
destruction. Drivers isolate unrelated error-stack topologies in fresh processes;
transfer oracle buffers use stable storage across protected error paths. This
keeps ASan instrumentation active in the runner without proc-based stack discovery.
The legacy `QUEUE_CASE=alt` alias still selects both registration/conflict groups;
the default runner launches `altregister` and `altconflicts` separately.

Availability stays 173/173 complete JIT+C entries and 166/173 guarded native
paths per priority architecture. No RV64-specific, MIPS/SPARC/PowerPC JIT, Tk or
security/crypto changes are included. Full native ABI work remains unfinished.
