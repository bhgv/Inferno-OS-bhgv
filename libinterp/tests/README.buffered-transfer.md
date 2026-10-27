# Native buffered SEND/RECV and checked interpreter ring geometry

This checkpoint adds genuine AMD64/ARM64 SEND and RECV payload copying and ring
publication. A shared C preflight only stages a private transaction; generated
code calls neither isend/irecv nor a C mover, allocator, scheduler or GC callback.
Availability increases from 164/173 to 166/173 active commands in both WORD
models. RV64 is unchanged. Per-command percentages and scopes are generated in
[README.jit-readiness.md](README.jit-readiness.md), with machine-readable rows in
[jit-readiness.csv](jit-readiness.csv).

## Accepted native path

The channel must be buffered, ready in the selected direction and have no
waiting peer. Native transfers support movb/movw/movl/movf, raw movm and mapped
movtmp with matching stride/metadata, np==0 and a size from zero through 256
bytes. Larger/mapped/custom movers, full sends, empty receives, unbuffered
channels and queued peers return to the existing C path. Blocking, ALT and
rendezvous are not implemented by these new leaves.

cchanleaf receives REG, direction and a private 64-byte ChannelLeaf transaction.
It checks decoded owner slots, channel/queue prefixes, buffer geometry,
descriptor/map/mover state, numeric payload ranges and receive reference limits without
changing VM operands, heap headers, queues or counters. Native-only restrictions
on nonzero maps and large widths are tested before walking maps, so a native
decline does not add a full pointer-map scan to the interpreter's work.

Payload aliases with REG, channel/Array Heap+body metadata or Type storage
decline. This preserves C's observable mutation order: C updates ring state and
a temporary buffer reference before copying, whereas native code publishes only
after copying. Overlap between the two payload spans remains supported.

Generated code copies complete qwords followed by exact byte tails, writes int
front/size values and publishes the captured R.s/R.d/R.m identities. Other REG
state, including PC, IC and t, stays unchanged. There are no calls after the
first payload store. Zero-width records do not access either payload, including
nil/H backing and operand identities. Receive temporarily retaining then
releasing a pointer-free buffer is unnecessary when no callback or owner
replacement can destroy it during the native transfer.

The SysV frame is 88 bytes, preserving REG across the helper call. The AAPCS64
frame is 80 bytes and also preserves LR. Both restore the complete stack on
acceptance and rejection. The production 128-u32 staging capacity is unchanged;
every short capacity must leave its output image untouched.

## Interpreter fixes

SEND/RECV now check complete decoded channel-owner slots, Channel headers and
peer-queue prefixes before queue access. Shared channelbuffer preflight validates
active rings before counter/reference changes: nonnegative lengths/counts,
int-representable capacity, front range, nonnil mover, Type size/np, map spans, marked pointer
bounds and backing-data arithmetic. External copy spans are checked before ring
publication. Invalid geometry raises a VM error instead of reading/writing a
negative or out-of-range slot and leaving a partially changed ring.

Full sends and empty receives retain lazy nonaccess of unused descriptors/data;
unbuffered H and C-null buffers retain their previous distinction. Ring-index
arithmetic never forms a signed front+size sum. Byte offsets use uintptr rather
than narrowing to 32 bits. Very large logical rings can still address one valid
selected slot, provided numeric arithmetic does not wrap. These are numeric span
checks, not verification of virtual mappings or allocation sizes.

## Validation

| New driver | Cases | Coverage |
| --- | ---: | --- |
| buffered-transfer-native.py | 1,593,088 | Real C, pure helper and production AMD64 RX; WORD32/64; full UBSan/NDEBUG, ASan successes/native declines |
| buffered-transfer-jit.py | 328,960 | AMD64/ARM64 machine instructions, helper ABI and publication; Python -O also passes |
| Total | 1,922,048 | Whole-image, registers, FP, stack, ring and decline oracles |

C/RX cases cover every byte width 0..256 for raw and pointer-free mapped movers,
all scalar identities, capacities 1/3/8, every front, partial/full readiness,
unaligned owner/value cells, both overlap directions, self-copy, operand-cell
aliases, zero-width poisoned data and INT_MAX ring geometry. The C helper is
checked separately for VM nonmutation and transaction canaries. Rejections cover
maps, malformed counters, descriptor widths, refs, peers, metadata aliases and
decoded/header/queue wrap. Guard failures verify exact VM error identities before
any image changes.

Guest tests independently stage the transaction, deliberately clobber legal
caller-saved registers, and verify argument registers, call alignment, exactly
one helper call, callee-saved registers, FP state, complete VM/payload images,
stack restoration, both near/distant REG layouts and code-capacity canaries.

The full error matrix passes in one process with UBSan. ASan keeps production
stack instrumentation enabled and passes successes/native declines with
BUFFER_NO_C_ERRORS=1. The combined ASan C-exception matrix is not a passing run:
this sandbox ignores __asan_handle_no_return and then reports stale stack
redzones when different call stacks are reused after longjmp. Exception cases
remain covered by the full UBSan run; no sanitizer instrumentation is disabled.

Thirty-eight selected driver configurations pass, including existing channel
transfers/creation/retention, mapped/raw copies, mapped moves and constructors,
array paths, Limbo layout/map checks and compiler publication. Publication now
checks SEND/RECV presence on AMD64/ARM64 and absence on RV64; the unsupported-only
module fixture uses EXIT instead of the newly supported SEND. This includes
both WORD models on all three compiler backends and RV64I/Zmmul publication.
Readiness generation additionally checks all 173 individual commands on both
architectures and WORD models with exact/short output buffers.

Replaying parent dd0f5d6 through BUFFER_C_SOURCE reproduces six focused failures:
SEND/RECV null decoded-owner reads, missing negative-front preflight and null mover calls. The
baseline driver selects BUFFER_BAD_ONLY, BUFFER_BAD and BUFFER_RECEIVE without
altering the parent source. The production changes retain C scheduling and
mapped ownership paths, tested by the existing FIFO/rendezvous/ALT regressions.

Physical ARM64/RV64 execution and full Inferno/compiler boot remain unverified.
Tk, security/crypto and MIPS/SPARC/PowerPC JIT sources are unchanged.

## Remaining native work

ALT, NBALT, SPAWN, MSPAWN, LOAD, EXIT and RAISE still have no native entry. Their
scheduler/error-frame/module ownership boundaries require dedicated paths and
regressions. SEND/RECV also retain C for queued, blocking and pointer-map cases;
opcode availability must not be reported as full semantic completion.
