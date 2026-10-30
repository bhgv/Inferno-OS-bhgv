# Bounded Dis64 vmachine execution and exception selection

The scalar constructor and opt-in file loader now have a connected gate through
unchanged production vmachine, rather than a fixture calling progexit after a
protected xec call. This is an execution/ownership test series: it does not add
opcodes, enable a calling/reference ABI, change default routing or define a
Dis64 control-section representation.

## Connected paths

`libinterp/tests/dis64-vmachine.py` borrows complete production functions and
reuses the existing guarded scheduler/file-loader builders. Two selections run
the same four immutable images: MOVW42, initialized ADDW2, zero DIVW and invalid
SHLW. Each selection executes both ordinary C and actual Linux/AMD64 RX leaves
and complete native tails.

The direct selection consumes an unpublished prepare64pilot owner. The file
selection uses actual readmod/parsemod with DIS_WORD32=1 and
DIS64_PILOT_LOADER=1; host read/stat/close replies remain controlled. readmod
releases its image buffer before actual root execution. Actual cache publication,
linkmod/newmp, schedmod/newprog/newgrp, PID/process/run queues, decoder/xec,
vmachine startup/error loop, handler/propex, progexit/delprog and stack/module
cleanup are connected in both cases.

No fixture calls progexit. Normal IEXIT and arithmetic errors enter the standing
vmachine error frame using actual emulator error/nexterror representation.
handler searches the actual current frame; an unhandled error reaches propex
and then the real terminal or retained-broken decision. Broken roots reach the
actual idle/Sleep branch, remain in the PID list and preserve their error. A
pending `fail:` kill then re-enters vmachine and cleans them through its real
selection path. The test stops at bounded OS cleanexit or Sleep boundaries;
only the fixture's deliberately abandoned standing error frame is reset there.

Two ready roots use quanta=1 and the unchanged VM round-robin rotation. A final
host exit is allowed only after both roots have actually been deleted. Separate
group cases use actual delgrp/addgrp/grpleader/propex/exprog to propagate an
arithmetic error from the leader or a follower. The recipient consumes its
pending kill before decoding an instruction: its MP stays at40 and no second
arithmetic handler/native call is observed. Blocked channels and notification-only
kill policy are strict unused boundaries, not simulated successful operations.

## Handler tables are legacy in-memory injection, not Dis64 wire support

The accepted scalar image has no reference map or control section. Separate
cases replace its entry descriptor **before scheduling** with an owned native
72-byte frame and a mapped reference slot at64, and attach owned legacy Handler/
Except tables directly to the Module. Source image bytes remain unchanged. This
is explicit runtime test setup, not admission of mapped Dis64 types or tables.

Eight cases per arithmetic image/backend cover:

- wildcard selection with a valid target and mapped result slot;
- the same selection with an actual cleanup map;
- misaligned result offset;
- an out-of-program destination with no usable fallback;
- a nonmatching exception pattern;
- an aligned but unmarked destination;
- cleanup metadata larger than the actual frame;
- an invalid named destination followed by a valid fallback destination.

Actual handler code transfers the process-held exception reference into the
frame, clears exval, publishes both live/saved PCs, resumes IEXIT and releases
the slot through actual destroystack. Independent retained owners check both
replacement and transferred references before and after terminal cleanup.
No freed Prog/Module is read. Actual modstatus is linked; controlled diagnostic
realloc failure exercises its existing nonfatal branch. Fresh string allocation,
newestring fallback, cross-frame unwinding and collector traversal are not claimed.

## Reproduction and evidence

```
python3 libinterp/tests/dis64-vmachine.py
CFLAGS='-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis64-vmachine.py
```

Both strict UBSan selections pass eight connected image/model runs plus two
negative controls. Default O1 executes117,616 active predicates; O3 executes
117,352. Counts include allocator/guard observations and are not an input coverage
percentage. In both builds the behavioral observations agree:

| Observation | Total across eight runs |
|---|---:|
| Actual handler calls |184|
| Selected handlers and resumed execution |24|
| Actual terminal Prog deletions |152|
| Retained-broken idle stops |8|
| Actual emitted native leaf calls |68|
| Accepted / declined native leaves |12 /56|
| Authoritative C handler calls |172|
| Actual complete-tail bridge calls |24|

The two controls replace only the borrowed propex body or handler-selection
body in generated test units. Each must first compile successfully and then
abort at an active runtime predicate; compilation failure or another exit is
not accepted as evidence. These prove the connected propagation/selection
checks actually observe those paths. Production source is not modified by the
controls. The shared source/registry and inspector execution_supported stay
unchanged.

Startup locking, namespace/file/environment services, final host exit and pools
are bounded single-worker fixture services. Pressure collection and other workers
are rejected if entered. No live GC, compaction, concurrency, debugger, complete
native interfaces, full emulator boot or physical ARM64/RV64 execution is proved.
The new-format loader still admits43 scalar opcodes only. Complex lifecycle,
linking and allocation work remains in C, reached through native tails where
available. Remaining integration should address complete build/native consumers,
then agreed calls/dependencies and reference/result/root ownership gates.

The later [GC-root checkpoint](dis64-gc-roots.md) separately runs the real
collector across exception-only cycles and packed native frames. It fixes
missing process exval tracing and unaligned frame metadata reads; it does not
replace this vmachine fixture's bounded GC service or enable Dis64 references.

The [exception/GC checkpoint](dis64-exception-gc.md) additionally combines
actual RAISE/handlers with collector epochs in a separate bounded native graph
fixture. This vmachine test retains its declared GC/OS boundaries.
