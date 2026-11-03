# Native64 series0099: cancel refresh publication before native Display teardown

Native closedisplay did not discard pending refresh records. Resource cleanup
can flush or try to queue notifications again, including after a prior native
write failure. The close path now marks a native closing phase under display
exclusion, detaches/frees private notifications before resource teardown and
performs final private cleanup before releasing the display lock/storage.
No callback payload destructor runs as a result of discarding a notification.

queuerefresh refuses new records before allocation while closing. flushimage
can still write native cleanup commands but does not run queued application
callbacks in this phase. refreshslave refuses admission while closing, rechecks
after input and between records, and exits after a callback publishes close.
These reads assume a valid independently retained Display and existing managed/
display exclusion. They do not make dereferencing a freed Display valid.

Image cancellation now takes display exclusion before reading/clearing callback
publication, clears both refptr and reffn, and removes matching queued contexts
under the same exclusion. Early empty/nil-context paths release an acquired
lock too. Clearing the native callback gate prevents late remote notifications
from calling an arbitrary function with a cancelled payload.

The new closing field is appended to the native C Display structure. All C
clients/libraries must be rebuilt together. This is NOT the managed Draw_Display
ADT, a shared LeafLayout field or a Dis64 schema extension: generated Frame64,
Heap/Type, managed Draw type shapes, opcode/wire IDs and file admission stay
unchanged. initdisplay already zero-initializes the complete native structure. Its lock
allocation refusal now frees the previously duplicated directory owner; this
small failure-path fix is code-reviewed/syntax-checked, not claimed as a full
initdisplay/device fault-injection run. Startup diagnostics now use declared
native print under the existing debug flag instead of undeclared printf.

native64-display-close.py executes the actual owning cancel, queue, flush,
lock/unlock, discard and close bodies with actual native structures/allocated
storage. Explicit image/channel/lock/I/O transports observe exact ordering and
attempt fresh enqueues/flushes during teardown. Tests cover forty cancellation
cases (duplicates, head/middle/tail/all/empty/no-match/nil; local/remote) and
eighty close cases (partial/full resources, local/remote execution, zero/many records and ordinary label
open/write refusals). Queue nodes and native resource tokens retire once;
callback payloads/default fonts remain caller-owned. The cancelled payload is
poisoned before processing survivors. Six strict controls omit admission,
initial discard, closing callback suppression, lock-before-publication or the
callback gate clear, or suppress required cleanup writes; each requires semantic10 before unsafe continuation.

Local O3 x64 nonrecovering UBSan with synthetic port aliases:5446 predicates
per WORD32/WORD32-file plus all six semantic controls. Updated actual refresh
worker/output tests pass1549790/model plus their four controls, including close
before reading, after a read and from the first callback of a batch. Complete
Draw scalar consumer still passes340506/model and model/initializer/result
controls. New native ARM64/x64 ASan and full ARM64/x64/RV64 gates include these
checks; their new target results remain separate evidence.

This is a serialized teardown phase, NOT a refresh-worker join protocol, a
lifetime pin, concurrent/double-close support or arbitrary destructor/VM longjmp
cleanup. Caller still must retain Display/callback storage and exclude or stop
other users before actual free. Device/namespace/collector/scheduler integration
and full Draw registration/AWM are not certified by bounded transports. Draw
export/frame coverage remains32/89; JIT coverage/file admission remain unchanged.

Preceding0098 full classic-mk41628016092 now passes native ARM64, native x64
and RV64/QEMU, in addition to its native target ASan evidence.

FLASHVM-0095 atf2471997 is preserved. Reviewed pinned7cc41d0f HEAP-POOL.md:
allocator metadata exclusion, source/payload ownership and collector admission
remain separate. Fault hooks run after the pool mutex unlocks; borrowed arena
walks still require stopped-world/quiescent admission, and stable block cursors
must not be invalidated by coalescing. No hosted allocator source imported,
local TSan replay or owning allocator/parallel-heap proof is inferred.
