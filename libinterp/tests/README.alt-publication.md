# ALT/NBALT guard publication across C and complete JIT entries

ALT used to publish a selection word or array index while it was still walking
the guard table or member array. Ready transfers also wrote their payload before
`xecalt` cancelled registrations. A result/tuple/payload overlapping a later
guard or member pointer could therefore change the remaining traversal, strand
waiters, dereference an overwritten channel, or reload a changed payload address.
The error cleanup repeated that unsafe traversal if a mover raised after writing.

The shared C implementation now captures a private `AltSelection`: concrete
channel, original payload/result/index addresses, guard ordinal, member ordinal
and transfer direction. `altchoose` resolves those fields without output stores.
Ready execution cancels this process's registrations, marks cancellation complete
in the protected context, then publishes the selection/index and calls the
ordinary SEND/RECV handler using the captured addresses. Neither success nor a
post-write mover error revisits the overwritten table. A nonblocking no-ready
selection likewise cancels before writing its default guard ordinal.

Blocked wakeup cancellation stages the selected payload/index while removing all
registrations; it validates the selected output cells and only then publishes the
wakeup. Invalid tuple or selection cells cannot stop cancellation halfway through
other guards. Pure cancellation does not initialize an unused selection snapshot.
Disabled H/nil array members cannot become a spurious selected member.

Only addresses are captured: a send still reads its payload after selection/index
publication. Result, tuple index and transferred payload keep that order. A mover
error may retain its ordinary partial data writes; this is not a payload rollback
transaction. Unrelated waiters keep their FIFO order and duplicate registrations
are all removed. No new allocation, public Alt/Altc/Prog field, opcode, Dis layout,
frame layout or GC map is introduced. Checks bound numeric spans, not arbitrary
virtual-memory ownership. This does not promise arbitrary aliases with channel
headers, queue nodes, scheduler objects or the private native stack.

ALT/NBALT remain deliberately complex C operations. AMD64 and ARM64 complete JIT
tails invoke these corrected handlers through the existing stable `cjitexec`
boundary; no native fast path or additional opcode is claimed.

## Verification

```
CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
QUEUE_JIT_ROUTE=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-transfers.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/gc-idle.py
```

For the complete new ASan/UBSan matrix, select the nine publication families:

```
QUEUE_CASES='publication-bad-ready publication-bad-wake publication-bad-output publication-idle publication-wake publication-wake-tuple publication-result publication-payload publication-tuple' CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-queues.py
```

Repeat with `QUEUE_JIT_ROUTE=1` for actual published AMD64 RX tails. Direct C now
executes the actual ALT/NBALT opcode wrappers too, so ALT's initial `R.t=0` is
represented in both routes. No handler or queue implementation is substituted;
the scalar mover, scheduler and allocation/error observations are controlled host
adapters. Actual emu/os protected-error helpers run in each WORD32/WORD64 profile
with native host pointers.

| Matrix | Scenario executions |
| --- | ---: |
| Nine new C families, four scheduler/WORD profiles | 71,716 |
| Nine new real RX-to-C families, same profiles | 71,716 |
| New ASan/UBSan total | 143,432 |
| Full queues, C and RX, UBSan/NDEBUG | 631,152 |
| Existing transfer matrix, UBSan/NDEBUG | 599,408 |

The new families vary packed table/result/member positions, scalar send/receive
and receive-array guards, counts/owner/operand/member aliases, foreign FIFO
waiters, ALT/NBALT ready entries, blocked wakeups, and errors after payload writes.
Independent complete byte images specify selection/index/data store order; queue
contents, outstanding node ownership, scheduler transitions, peer data, IC/t and
selected wakeup pointers are checked. Three rejected-cell families additionally
check nil, H and wrapping result/index slots. Their child predicates are extra
checks outside the scenario counters.

All nine selections fail at runtime on the parent `75bc786` implementation, both
directly and through real RX tails: **18 independent parent configurations**.
Use `ALT_SOURCE=/path/to/parent-alt.c`, `QUEUE_CASE=<family>` and optionally
`QUEUE_JIT_ROUTE=1`. The parent is borrowed verbatim; these failures are not
fixture compilation failures. An invalid wakeup result independently exposes
leftover registrations as well as incomplete wakeup publication.

The added rejected-cell jump topologies run in fresh children; fixture error
jumps and child exits use indirect calls to avoid the sandbox libasan's repeated
abandoned-stack cleanup problem. Production and fixture stack instrumentation
remain enabled, and no shadow memory is manually unpoisoned. A full legacy ASan
run still reaches libasan's own stack-cleanup failure in `altgeometry`; all legacy
families run under UBSan/NDEBUG, and all new families pass ASan/UBSan. Leak process
inspection remains disabled by the inherited fixture.

The native C/RX bridge retains 177,152 routes, 6,183 compiler cases and 295
integration checks per WORD model; ARM64 compiler preflight retains 6,215 per
model. Incoming GC-idle fixture changes were preserved and all three 980-case
variants pass. Complete ALT source syntax-checks against production headers.
Unicorn/capstone are absent in this workspace, so the optional ARM64/AMD64 guest
instruction-emulation driver was not run in this series. Native RX execution is
AMD64; this is neither physical ARM64 validation nor a complete 64-bit VM boot.

Fresh availability remains 173/173 complete JIT+C operations and 166/173 guarded
native fast paths on each priority architecture. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are unchanged.
