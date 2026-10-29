# ALT cancellation after channel finalization drains a queue

Both emu and os `killcomm` detach and free each queue head before waking its
process. Removing the final registration legitimately leaves the channel's
queue pointer nil. For a blocked ALT, `killcomm` calls `altgone`, which cancels
that process's registrations on every guarded channel.

Scalar-send cancellation and receive-array cancellation previously dereferenced
that drained queue unconditionally. A channel with just one blocked sender or
receive-array participant could therefore fault during finalization. Other
registrations could remain stranded and the process would not reach its hangup
wakeup. The scalar-receive path already handled the nil queue.

The shared `altdone`/`altunmark` cancellation paths now skip a queue already
drained by finalization while continuing to remove the process from all other
queues. This is a teardown state, not permission to use nil queues in readiness
or registration; those checks remain strict. Ordinary selection, payload/index
publication, hangup strings, scheduler APIs and native layouts are unchanged.

## Executable integration

The queue driver extracts the actual `killcomm` from each of emu/port/dis.c and
os/port/dis.c, and the actual `freechan` and `freetype` from heap.c. Queue allocation
and freeing are counted. Host scheduling and Prelease interruption use explicit
adapters; this is not execution of host process locks or a full GC sweep.

`alt-hangup` blocks one to four independent processes using real ALT, directly or
through its complete AMD64 JIT C tail. Scalar sends, scalar receives and array
receives cover all three finalized channels, one to four duplicate guards, packed
guard/member storage, mapped/nonmapped mover identity, direct queue draining
versus `freechan`, and an unrelated waiter on a surviving channel. Each process
has its own guard table, Array header, member cells, payload and result storage.

The tests require exactly one wakeup per ALT participant, the existing hangup
reason, nil payload pointers, unchanged saved REG/guard/member/output images,
complete removal of all duplicates, and preservation of the unrelated waiter's
queue and state until its own channel is finalized. Real `freetype` decrements
mapped channel descriptors, retaining the one external reference. Buffers are H:
buffer payload destruction and actual collector ownership are outside this test.
No mover may execute during hangup.

`queue-hangup` separately exhausts ordinary Psend/Precv/Prelease state combinations
through four processes and checks empty queues, both directions, raw killcomm and
freechan, the existing channel-hangup reason and native interrupt delegation.

| Family | Cases per runtime/WORD profile | Across emu/os and WORD32/64 |
| --- | ---: | ---: |
| alt-hangup | 30,720 | 122,880 |
| queue-hangup | 484 | 1,936 |
| New total | 31,204 | 124,816 |

The complete queue/ALT suite passes 695,832 UBSan/NDEBUG scenarios directly and
with actual AMD64 complete-tail ALT entry. The isolated new families also
complete ASan/UBSan; sandbox libasan prints executable-name/symbolization warnings.
The inherited channel-transfer suite passes 640,576 cases. General AMD64 complete
dispatch, ARM64 compiler bridge, readiness and dis2c differential suites pass.
ARM64 remains compiler-only here; no ARM64 machine code was executed.

Parent `60837e4` compiles but UBSan fails inside cancellation for both controls:

```
ALT_SOURCE=/path/to/parent-alt.c QUEUE_CASE=alt-hangup HANGUP_FORM=0 HANGUP_PEERS=1 HANGUP_DUPLICATES=1 python3 libinterp/tests/channel-queues.py
ALT_SOURCE=/path/to/parent-alt.c QUEUE_CASE=alt-hangup HANGUP_FORM=2 HANGUP_PEERS=1 HANGUP_DUPLICATES=1 python3 libinterp/tests/channel-queues.py
```

Run the new matrix with:

```
QUEUE_CASES='alt-hangup queue-hangup' CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
QUEUE_JIT_ROUTE=1 QUEUE_CASES='alt-hangup queue-hangup' python3 libinterp/tests/channel-queues.py
```

Optional filters are HANGUP_FORM=0..2, HANGUP_PEERS=1..4,
HANGUP_DUPLICATES=1..4 and HANGUP_TRIGGER=0..1. Coverage remains 173/173 JIT+C
commands, 166/173 guarded native fast commands and seven C-only complex commands
for AMD64 and ARM64; this series fixes existing shared behavior.
