# Native channel entry, REG restoration and temporary-owner cleanup

Native `csend`/`crecv` in emu and os previously restored REG only after a ready
instruction returned normally. A mover's VM exception bypassed restoration.
`csendalt` also saved too late to cover buffer allocation and restored too early
to cover payload destruction; callbacks in either stage could corrupt the native
caller's decoded register state, including its PC/module/scheduler fields.

`include/vmchannel.h` now shares checked readiness and a protected ready-transfer
entry. It validates channel alignment/extent, both complete queue heads,
peer Prog extent, buffer sentinel/span and count/capacity consistency before
native callers register a waiter, allocate or replace REG. These are numeric
extent/layout checks, not proof that arbitrary pointers are mapped.

Ready `csend`/`crecv` capture the original native addresses, run the authoritative
SEND/RECV instruction under `kprotect`, restore the complete REG byte image and
then rethrow the original VM error. Byte copies explicitly preserve native
structure padding as well as fields. Native blocking paths still call `ccom`
without restoring an obsolete pre-wait snapshot: acquire/irestore must keep its
resumed process state and scheduling quantum.

`csendalt` owns a protected context spanning readiness, optional buffer allocation,
actual send and payload cleanup. Original addresses, descriptor, capacity and
result live outside the nonlocal error frame. REG is restored after the whole
operation on both exits. Normal drop/success still consume the input's mapped
references exactly once; exceptions do not trigger a second unconditional
payload cleanup. Existing partial transfer/destructor effects are not rolled
back, and callers must not blindly retry a partly completed operation.

The same callback/error matrix exposed two temporary-owner leaks in shared C
receive paths. `cgetb` now releases its retained Array on both protected exits.
Queued-sender receive/refill likewise releases its retained Channel when either
stage raises. The refill context captures the receiver destination and sender
source before callbacks borrow REG. Neither change promises transactional ring,
peer-queue or payload rollback; the ordinary partial-publication/error semantics
remain, with temporary references balanced.

There is no public channel, Prog, frame, Dis, GC-map or JIT ABI change. The new
contexts are private C stack objects. Shared-header dependencies are explicit in
libinterp and both port makefiles. These native APIs preserve REG; payloads that
alias REG itself or the private native stack are outside that preservation
contract. The blocking route still requires the VM acquired and an ordinary
native scheduler participant.

## Verification

```
CFLAGS='-O1 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-transfers.py
python3 libinterp/tests/channel-queues.py
python3 libinterp/tests/mapped-moves.py
python3 libinterp/tests/buffered-transfer-native.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 utils/dis2c/tests/test.py
```

The six new transfer families execute actual emu/os `csend`, `crecv`, `ccom` and
protected-error helpers, shared runtime SEND/RECV/movers/heaparray/freeptrs, and
native `csendalt`. Scheduler, allocator and destructor observations are controlled
host adapters. Callback injections borrow every REG byte and raise before/after
real scalar/pointer mover publication, during allocation, or during source cleanup.
Independent packed source/destination positions span all eight byte alignments.
Temporary references, descriptor counts, node/heap ownership, native outcome,
error identity and the complete original REG image are checked.

| New family, per scheduler/WORD profile | Scenarios |
| --- | ---: |
| Ready native send/receive/best-effort movers | 2,304 |
| Allocation borrowing/error | 256 |
| Payload destruction borrowing/error | 256 |
| Blocking resume/error contract | 4 |
| Invalid channel/buffer/queue/peer readiness | 48 |
| Queued buffered receive/refill, both error stages | 768 |
| Per profile | 3,636 |
| emu/os, native pointers with WORD32 and WORD64 | **14,544** |

The complete inherited+new transfer matrix passes **613,952** scenarios under
UBSan/NDEBUG. Queue/ALT, mapped moves, actual AMD64 buffered C/RX, general C/RX
bridge, ARM64 compiler preflight, readiness and AOT loader/decoder/xec integration
also pass. Existing mapped-copy fixtures now implement nested protected error
frames instead of passthrough callbacks, so the new temporary-owner cleanup is
actually exercised before errors reach their outer oracle.

The non-throwing native matrix additionally passes **5,128** scenarios under
ASan/UBSan/NDEBUG with stack instrumentation enabled:

```
CHANNEL_TRANSFER_CASES='entry-move entry-allocate entry-destroy entry-block entry-refill' ENTRY_MODE=1 CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-transfers.py
```

Full injected-error ASan runs encounter the sandbox libasan abandoned-stack
cleanup failure; their complete error matrix uses UBSan. Fixture jumps use an
indirect call through the real VM error buffers to avoid duplicate compiler
no-return cleanup. No production/fixture stack instrumentation is disabled or
shadow memory manually unpoisoned. Leak task enumeration remains unavailable;
explicit ownership counters and descriptor/reference checks remain active.

Twelve selected parent configurations fail during execution against `63abd22`:
emu send and os receive mover errors; best-effort mover/allocation/drop/cleanup
borrowing/errors; receive Array retention; queued receive/refill Channel retention;
and invalid native channel/buffer entries. `XEC_SOURCE` selects the parent xec
body and `CCOM_SOURCE` selects the parent emu/os native entry bodies. Select a
family with `CHANNEL_TRANSFER_CASE=entry-move|entry-allocate|entry-destroy|entry-refill|entry-invalid`;
`ENTRY_API=0|1|2`, `ENTRY_MODE=1|2|3`, `ENTRY_BUFFERED=0|1`,
`ENTRY_POINTERS=0|1`, `ENTRY_STAGE=1|2`, and `ENTRY_BAD=0..15` isolate probes.
Examples that independently reproduce the temporary-owner leaks:

```
XEC_SOURCE=/path/to/parent-xec.c CHANNEL_TRANSFER_CASE=entry-move ENTRY_API=1 ENTRY_MODE=2 ENTRY_BUFFERED=1 ENTRY_POINTERS=0 python3 libinterp/tests/channel-transfers.py
XEC_SOURCE=/path/to/parent-xec.c CHANNEL_TRANSFER_CASE=entry-refill ENTRY_MODE=2 ENTRY_STAGE=2 ENTRY_POINTERS=0 python3 libinterp/tests/channel-transfers.py
```

The complete production xec source and emu native entry file pass host syntax
checks with their appropriate headers/defines. Both priority backends retain
173/173 complete JIT+C operations and 166/173 guarded native fast paths; no new
opcode or native fast implementation is added. The corrected buffered C paths
serve complex SEND/RECV fallbacks. Native RX execution is AMD64 and ARM64
validation is compiler-only in this series; full native VM boots and physical
ARM64 execution remain unverified. Tk, security/crypto and MIPS/SPARC/PowerPC
JIT sources are unchanged.

## Direct SEND/RECV readiness and both queue heads

Direct interpreter instructions and JIT C fallbacks now use the same readiness
validation as native C entry. Previously `channelpeer` only checked a queue's
`prog` prefix: a misaligned head, wrapping complete head, or invalid Prog could
reach typed queue/scheduler operations. A malformed opposite queue could also
reach waiter registration. Both queues now require aligned, nonwrapping complete
headers, with nonnil peers covering a complete aligned Prog. Buffer sentinel,
negative counts and count/capacity mismatches reject before queue changes.
No queue-chain traversal or mapped-address proof is implied by these checks.

`direct-invalid` adds 4,608 cases per runtime/WORD profile (18,432 total): two
instructions, two independently damaged queue directions, 18 malformed metadata
variants and all 8-by-8 channel-cell/payload alignments. It checks error identity,
the complete REG byte image, payload canaries, allocation/node counts and scheduler
transitions. The parent `bfa5253` fails this matrix at runtime. All 632,384 channel
transfer scenarios pass under UBSan with NDEBUG; 315,576 queue/ALT scenarios also
pass. The isolated new matrix also completes with ASan/UBSan, although this
sandbox's libasan emits its existing stack/no-return warnings; this is not a claim
that the full injected-callback exception matrix passes ASan.

The pure buffered JIT preflight matches both-head checks without raising. It also
excludes native transaction scratch from both complete queue heads, including
`next`, so preparing a transaction cannot overwrite queue metadata. The AMD64
RX/C suite passes 796,128 accepted and 640 declined cases per WORD model, plus
2,414/2,430 private preflight cases for WORD32/64. The added 224 declines per model
cover nil/H/misaligned/wrapping heads and invalid peers in both directions; each
also checks the authoritative instruction's unchanged-state error. The parent
pure preflight fails `BUFFER_BAD_ONLY=1 BUFFER_BAD=34` at runtime.

Coverage remains 173/173 JIT+C commands and 166/173 guarded native fast commands
for AMD64 and ARM64. This series changes shared channel correctness, not the count
of native opcodes. ARM64 validation is compiler-only (6,215 checks per WORD model);
actual RX execution here is AMD64.
