# ALT metadata preflight before registration

ALT previously registered early guards while it was still inspecting later
channels or channel-array members. Unlike direct SEND/RECV, its readiness scans
read queue fields and Array/Channel heap identity without the shared header/count
checks. A later bad queue could raise during registration and then be revisited
by `altdone` while unwinding, replacing the original error or dereferencing the
same malformed data. Negative unbuffered counts could also pass ALT preparation.

`altvalidate` now checks every guard and receive-array member before the first
registration or allocation. `altobject` checks alignment, nonwrapping object
extent and numeric room for the preceding Heap before reading its Type identity.
`altchannel` applies common readiness checks and validates both complete queue
chains, including cycles, internal empty nodes and invalid peer extents. A
validation exception escapes before cleanup owns any newly registered guards.
The ordinary protected registration/communication cleanup remains in place for
later allocation or mover errors.

Top-level nil/H guards and nil array members retain their nil-reference error.
H array members remain inactive. Empty receive arrays still require no readable
data buffer, and packed member cells/ALT tables are read with byte copies.
Arrays cannot serve as send channels or nested concrete channel members. Invalid
array geometry raises the ordinary bounds error before walking members.

This is numeric/layout validation, not mapped-address proof or validation of
Heap/Type ownership. It does not snapshot mutable guard tables across a blocked
ALT, change payload/selected-tuple checks, promise transactional transfer rollback,
or add a new public ABI. The existing ALT wrapper clears REG.t even on error;
NBALT retains it. No JIT backend or opcode count changes.

## Executable matrix

`alt-preflight.c` is injected into the existing queue fixture. `QUEUE_CASE=alt-metadata`
selects the new family; `ALT_META_BAD=0..31` and `ALT_META_FORM=0..2` narrow it.
The three forms are scalar send, scalar receive and receive-array member.

The matrix covers all three bad-guard positions, independent 8-by-8 table/member
alignments, ALT/NBALT, and empty versus unrelated populated queues. Variants cover
nil/H/unaligned/wrapping owners and queues, invalid peers, cyclic/malformed tails,
buffer sentinel/count errors, malformed arrays, nested arrays, misaligned Array
headers and array-as-send misuse. Existing valid/empty-array and publication
alias matrices continue to run.

There are 61,440 new cases per emu/os and WORD32/64 profile, 245,760 total. Each
checks the original exception identity, zero registration allocation attempts,
unchanged queue nodes/owners, channel metadata, guard/member/output bytes and
process state, plus REG preservation apart from ALT's established t reset.

Verification completed:

| Suite | Result |
| --- | --- |
| Queue/ALT UBSan + NDEBUG | 571,016 scenarios |
| Queue/ALT through native AMD64 complete dispatch | Same 571,016 scenarios; ALT/NBALT use their real C tails |
| New metadata family ASan/UBSan + NDEBUG | 245,760 scenarios completed; sandbox libasan emits existing no-return/stack warnings |
| Channel transfer UBSan + NDEBUG | 640,576 scenarios |
| General complete AMD64 C/RX dispatch | 177,152 routes, 6,183 compiler and 295 integration checks per WORD model |
| ARM64 compiler bridge | 6,215 checks per WORD model; no ARM64 machine-code execution |
| dis2c | Real loader/xec/decoder, all opcode dispatches, 144 addressing modes and malformed/runtime differential checks |
| JIT readiness | 173/173 JIT+C, 166/173 guarded native fast commands for AMD64/ARM64 |

Parent `b27a851` compiles but fails the executable negative controls:
`ALT_META_BAD=7` incorrectly returns normally for a negative count;
`ALT_META_BAD=9 ALT_META_FORM=0` faults in cancellation on a nil queue;
`ALT_META_BAD=29 ALT_META_FORM=2` faults when treating a nested Array as a Channel.
Source overrides use `ALT_SOURCE` in the existing driver.

```
QUEUE_CASE=alt-metadata CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
QUEUE_JIT_ROUTE=1 QUEUE_CASE=alt-metadata python3 libinterp/tests/channel-queues.py
QUEUE_CASE=alt-metadata CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
```
