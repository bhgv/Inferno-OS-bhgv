# Private canonical return-store proof: checkpoint0080

`include/dis64-bound-return.h` extends the private actual-byte analysis with a
conditional nonvoid return postcondition. It does not enable executable file
calls, general indirect memory, imports or new format identifiers.

## Exact store role

The private reader recognizes only existing MOVW/MOVL/MOVF with direct AFP or
compact immediate source, AXNON middle and indirect AFP destination encoded as
base32 THEN secondary0. This is the existing classic address framing for
`0(32(fp))`, confirmed against `limbo/dis.c:disaddr`; it does not change the
production direct-only Dis64 reader. Real immediates refuse. Result opcode and
source declaration kind must match the canonical export signature exactly,
including int/bool4 and big/real8 distinctions. Bool literals are0/1 only.

The incoming byte32 address is a PRECONDITION: it must be installed by a checked
caller as the live exclusive caller-owned result cell. This constructor proves
what the callee body does under that premise, not that a raw pointer or caller
binding is valid. Compiler declaration provenance and actual container-section
provenance remain trusted obligations as in0079. Incoming Bool arguments must
also satisfy the checked caller's canonical value invariant.

Every byte and role is checked before analysis, including unreachable stores.
No root/void result stores, secondary offsets, different pointer bases, source
indirection, result-address mutation, arbitrary pointer copies or result-tail
reads are admitted. Existing ordinary numeric operands still exclude frame
header, holes and outgoing result tail. Calls/FRAME/LEA remain closed.

## Numeric reads and return coverage

Canonical raw exports/pool/types bind each function identity, interval, frame and
signature. Numeric definite initialization runs first on the actual facts;
a result store reads its source but does not initialize a numeric local. Source
reads precede any result write. The128 numeric slots remain fully available;
no numeric bitmap bit is stolen for result assignment.

The second C pass starts at entry and cuts traversal AFTER a valid typed result
store. A RET reachable in that cut graph means a return path can bypass every
result write, so it refuses. This is valid because this subset admits no call
or operation that can invalidate the stored result before RET. Distinct entry
paths, loops and multiple stores are handled without an execution-termination
assumption. Only entry-reachable stores/RET count in the summary.

For a conservative initial policy, nonvoid functions must have at least one
entry-reachable RET. Void closed loops remain valid. A reachable RET on one arm
does NOT prove other arms terminate; infinite loops may remain. Unreachable
nonvoid RET cannot satisfy that existence condition. This matches the witness
proposal in FLASHVM-0069. It is private verifier policy, not a wire requirement
or loader activation.

Summary `numeric.writes` counts numeric writes only; `stores` separately counts
entry-reachable typed result stores. `returns` counts reachable RET, not executed
returns. Final function records/count publish atomically after the entire module
passes; scratch/queue are disposable. No allocation or recursion is used.

## Work and reference

At most64 functions,65536 total instructions,128 slots/function and1MiB/raw
section remain. Caller scratch is the existing numeric scratch plus one queue;
return cover reuses completed queue flags. A cut node costs1 plus its traversed
edge roles; stopping at a store traverses zero outgoing roles. Each cut node is
queued once, so additional work is at most3N. The aggregate module quota charges
numeric work PLUS return work, bounded by134*65536 =8781824. This is verifier
information/traversal work, not guest quota, CPU time or a serialized limit.

Run `python3 utils/tests/dis64-bound-return.py`. The independent reference checks
raw indirect framing, validates canonical metadata through the prior independent
catalog oracle and uses its synchronous numeric incoming sets. Reference-only
store-to-next-jump normalization preserves PCs/edges/numeric writes; source reads
are checked explicitly against the exact incoming sets and added to read counts.
A separate synchronous TOP/intersection boolean solver proves the return bit;
independent DFS computes the cut-work charge only after that proof succeeds.
Neither normalization nor a supplied fact graph enters the C constructor.

The1548-vector corpus includes937 return-body cases and611 prior catalog/numeric
fixtures under the stronger contract:388 accepted/1160 refused. Coverage includes
all scalar kinds, pointer base/displacement, source spans/initialization, all256
mode bytes, nonminimal compact operands, all raw section prefixes/trailers,
unequal branch coverage, cycles/delayed paths, bit128, reachable/unreachable RET,
late refusal guards, exact/below quota,64/65 functions and65536/65537 instructions.

All26 active removal controls must reach semantic status10, not compiler/signal/
sanitizer failure. Fourteen retain canonical binding/role/capacity/atomic tests;
twelve target address base/displacement, store opcode, Bool, void store, source
read, false numeric-store credit, missing branch, missing RET check, return
existence/reachability and return-work charging.

[Target report](native64-series0080.md). Actual checked caller preparation,
result-address provenance, return retirement and shared whole-file C/RX call
goldens remain before loader/VM/JIT admission.
