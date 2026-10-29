# Channel transfer, ready-alt and FileIO regressions

Subsequent [native channel entry checks](README.native-channel-entry.md) restore REG on callback errors and balance temporary Array/Channel retains.

Run from the repository root:

```sh
python3 libinterp/tests/channel-transfers.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libinterp/tests/channel-transfers.py
```

The fixture extracts production buffered get/put, send/receive, scalar/pointer
movers, channel queues, complete alt.c, csendalt, heaparray and its allocation,
initialization and reference-map helpers. Heap allocation is bounded; reference
destruction and scheduler transitions are adapters. The real emu/os protection
bodies and srvunblock FileIO caller are extracted for each runtime configuration.
WORD64 runs 140,434 scenarios and WORD32 runs 140,430, for 561,728 across both
runtime adapters. ASan/UBSan and explicit heap/node/type-reference counts pass.

The FIFO matrix covers capacities 1 through 16, every initial ring position,
and 512 deterministic mixed operations per case against an independent queue.
Scalar and pointer payloads must retain order, wrap correctly and free replaced
values. Full-buffer tests queue two additional senders, then check oldest-first
reception, refill, wake order and sender-pointer cleanup.

Ready-alt tests cover send/receive, actual queued peers, available buffers,
scalar guards, receive arrays with nil and duplicate members, non-ready guards,
result indices and cleanup. Another matrix blocks an actual alt, switches the
modeled current process, communicates through the production opposite instruction,
and verifies wakeup, saved-result selection, array indices, payload transfer and
removal of every duplicate registration. This models two VM contexts; it does
not run two concurrent threads or a full interpreter scheduler.

Native best-effort sends are swept over capacities zero through three, every
initial occupancy, and requested lazy-buffer limits -2 through three, for both
scalar and pointer data. Rejected sends consume payload references exactly as
successful sends do, preserve all VM registers, and neither allocate nor block.
A waiting receiver accepts the message regardless of the lazy-buffer limit.
Positive limits can allocate a buffer; allocation errors retain the input and
old registers/buffer, and permit retry. Nil channels retain the original error.

The real srvunblock callers in emu/port/devsrv.c and os/port/devsrv.c use -1 for
close notifications. With no waiting receiver, both read/write notifications
must complete without allocating a negative array or leaving the interpreter
lock held. These tests run with native WORD64 layouts. The temporary WORD32
model keeps native pointers, so its generated FileIO map quantum is not an
actual 32-bit FileIO ABI and that integration check is skipped there.

csendalt now returns failure after consuming the payload when an unbuffered
channel has no receiver and the requested buffer limit is nonpositive. Previously
negative limits raised a heap error and zero limits could block after creating
an empty buffer. Full existing buffers retain the same drop result, without
an unconditional diagnostic print. No-ready service-close tests fail against
the old csendalt; the separate queue fixture rejects the old self-alt check.

CHANNEL_TRANSFER_CASE selects fifo, senders, native, ready, wake, service or
faults. CHANNEL_NATIVE_LIMIT restricts the native limit sweep, allowing the
zero-capacity failure to be reproduced independently of negative limits. XEC_SOURCE and ALT_SOURCE select alternate production bodies. These
checks do not establish full Inferno boot, hardware thread scheduling, GC
interleavings, native JIT call/exception ABI integration or a new 64-bit backend.
WORD32 uses real declarations with scalar WORD/UWORD substitutions while host
pointers stay native. Dis layouts and instruction encodings are unchanged.

The scalar/pointer sendvalue and receivevalue helpers now place channel owners
and source/destination values in independently varying byte offsets. Existing
140,462 WORD64 and 140,458 WORD32 scenarios per scheduler also compare the
surrounding slot bytes. Both emulator and native-kernel variants pass ASan/UBSan.
This checks the production byte-safe isend/irecv and movp paths; no scheduler
or target OS boot is simulated by these host fixtures.
