# Channel queues and failure-safe registration

Run from the repository root:

```sh
python3 libinterp/tests/channel-queues.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libinterp/tests/channel-queues.py
```

The driver executes production cqadd/cqdel/cqdelp, isend/irecv, the complete
alt.c implementation, and ccom from both emu/port/inferno.c and
os/port/inferno.c. It uses real Heap/Channel/Array/Alt/Prog declarations and
Proc error frames. Both runtime kprotect/krethrow bodies run through the
emulator error-stack adapter, in WORD64 and WORD32 scalar models. Each of the
four configurations passes 29,945 scenarios, with explicit balanced queue-node
allocation accounting and ASan/UBSan checks.

The FIFO sweep exhausts every queue of length zero through eight over three
process identities, removing each possible target. It checks exact survivor
order, repeated removal, append, complete drain, and reuse. Empty queues must
retain exactly one empty node; occupied queues must have no empty nodes.
The old cqdelp can delete every node when duplicate registrations match.

Send and receive tests fill the waiting queue, fail its next allocation, and
require the current process to stay runnable with unchanged payload pointer,
channel and VM scheduling registers. Success must queue the process exactly
once and then enter the corresponding blocked state. Runnable-list transitions
are stubbed; the production instruction and queue logic are extracted intact.

Alt tests cover repeated send/receive guards, receive arrays with repeated
channels and nil members, nil scalar guards, self-communication rejection,
blocking and default branches, empty and occupied waiting queues, and failure
at every registration allocation. Errors must cancel partial registrations,
preserve other waiters and the result slot, and balance nested error frames.
Successful blocked registration is cancelled explicitly by the test. Ready
communication selection and buffered transfer are tested separately by
channel-transfers.py. Actual concurrent scheduler execution remains outside
these fixtures.

Native ccom tests fail queue allocation, inject interruption while blocked,
simulate prior killcomm removal, and complete a normal wakeup. Every case
checks payload-pointer cleanup and preservation of unrelated waiters.
The error frame now covers cqadd as well as cblock.

Set QUEUE_CASE to queues, blocking, alt, or native to select a failure family.
XEC_SOURCE, ALT_SOURCE and CCOM_SOURCE allow comparison against old bodies.
Each family's pre-fix body independently fails its scenario. These fixtures
model channel allocations and scheduler boundaries; they do not boot Inferno,
exercise true concurrent scheduling, or validate native-kernel integration.
WORD32 retains host pointers and is not a complete 32-bit ABI execution test.
No Dis layout or native JIT instruction encoding changes are introduced.

The self-communication matrix now places zero through three unrelated receivers
before this process, using scalar guards or arrays with repeated channels and a
nil member. Rejection must occur before data transfer; exact foreign FIFO order
and all process/result state remain unchanged. The previous head-only test
incorrectly admitted a conflicting alt when a foreign receiver came first.

The blocking group additionally checks 32 nil/H owner cases per scheduler/word
combination at all byte alignments. It asserts the VM nil-reference error before
queue allocation and checks unchanged data, decoded registers and operand bytes.
The focused blocking group passes ASan/UBSan in all four combinations.

## Chain validation before append and cancellation

`cqadd` used to allocate before walking the queue. A cyclic chain could loop
forever after allocation, while a damaged tail could fault with the new node
still unpublished. `cqdelp` could remove/free earlier matches before reaching a
bad tail, or loop forever when the target was absent from a cycle.

The shared `vmchanhead`/`vmchanend` helpers now validate the owner cell, complete
aligned nodes, peer spans and the permanent-empty-head invariant. A one-link-read
Brent scan detects cycles without allocation or recursion. Append validates and
captures the existing tail before allocating; cancellation validates the whole
chain before removing any match. Both remain O(n); cancellation uses a validation
pass followed by its removal pass. FIFO dequeue stays O(1), checking only the
head and immediate successor before changing the owner/freeing the former head.
This does not promise full-tail validation for dequeue, mapped-memory proof,
repair of corrupt queues, or rollback after arbitrary allocator/free callbacks.

An empty node with a successor is invalid even in readiness checks. Both native
C callers and direct SEND/RECV reject it before transfer/registration; the pure
buffered JIT helper declines it without modifying the private transaction or VM.
No public queue/Prog/REG/Dis layout or calling convention changes.

`queue-chain.c` adds four independently selectable families:

| QUEUE_CASE | Cases per runtime/WORD profile | Purpose |
| --- | ---: | --- |
| queue-chains | 2,174 | Interior metadata errors and every cycle entry through length 16 |
| queue-slots | 32 | Invalid root cells, heads and append/cancellation targets |
| queue-pop | 10 | Constant-time head/successor rejection before freeing |
| queue-long | 204 | Valid lengths through 257, duplicates, absent targets and failed append |

The 9,680 new cases across emu/os and WORD32/64 check exception identity, complete
node/REG preservation, unchanged root and balanced allocations/error frames. Queue
families have a 60-second process timeout so a cycle regression cannot hang CI.
All 325,256 queue/ALT scenarios pass UBSan with NDEBUG; the native AMD64 ALT route
also passes the inherited matrix and all four new families. The isolated 9,680
new cases complete ASan/UBSan, with the sandbox's existing libasan no-return/stack
warnings; this is not a clean-ASan claim for every injected callback test.

Controls using parent `7e4613b` compile and fail at runtime for malformed append,
cancellation and dequeue. The parent's absent-target cancellation on a cyclic
queue times out. Its pure buffered preflight fails the new `BUFFER_BAD=41` case.

```
QUEUE_CASES='queue-chains queue-slots queue-pop queue-long' CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
QUEUE_JIT_ROUTE=1 QUEUE_CASES='queue-chains queue-slots queue-pop queue-long' python3 libinterp/tests/channel-queues.py
```

See [ALT metadata preflight](README.alt-preflight.md) for validation before the
first registration, malformed later-guard regressions and direct/JIT C-tail checks.

See [channel finalization and blocked ALT](README.alt-hangup.md) for drained-queue
cancellation, actual emu/os killcomm integration and duplicate/multi-process checks.
