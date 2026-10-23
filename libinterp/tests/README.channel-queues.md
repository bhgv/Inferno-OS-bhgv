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
four configurations passes 29,914 scenarios, with explicit balanced queue-node
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
communication selection, buffered transfer and actual scheduler wakeups are
outside these alt tests.

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
