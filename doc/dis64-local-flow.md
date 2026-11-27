# Experimental local control flow: checkpoint 0074

Status: private abstract-control preflight, not wire admission or a complete
file-call profile. This builds on the 0073 catalog but deliberately consumes
only its interval boundaries. Inputs are abstract nodes with an existing ISA
opcode and an explicit control target. They are NOT Dis64Instruction records,
wire operands or a new encoded section. The loader/compiler/pilot do not use it.

The checker revalidates a complete contiguous partition of instruction indices.
Every node, including unreachable code, must have an owner. Branches and jumps
stay in that owner's interval. A conditional edge needs a valid fallthrough as
well as a local target. Numeric/frame/address operations need a local next node;
CALL requires an exact function entry and a local return continuation. RET/EXIT
are terminal. Existing unknown/unsupported operations and noncanonical unused
abstract target fields refuse. Closed intrafunction jump loops are allowed:
this is not a termination proof.

Call sites build a deduplicated function-edge bitset. A bounded nonrecursive
Kahn traversal rejects cycles anywhere, including disconnected functions, and
computes the longest path over ALL declarations. The explicit depth limit is
part of the draft resource policy. Repeated call sites count separately but do
not inflate indegrees. Scratch is fixed: a 512-byte bitset and bounded 64-element
arrays; no heap allocation or recursion. Every failure leaves the output summary
and its guards unchanged. No result is published until all edges and the whole
call graph pass.

An independent Python DFS oracle supplies1540 cases:324 accepted/1216 refused.
They include all18 conditional branch families, boundary/backward/self targets,
interior calls, cross-function edges, final-instruction fallthrough, repeated
call sites, disconnected cycles,64-function chains, exact/exceeded depth limits,
malformed partitions, unknown opcodes, and65536/65537-node resource boundaries.
Five removal controls separately weaken branch ownership, fallthrough, exact
call entries, cycle detection and depth limits. Each must hit explicit semantic
status10, not a signal, compiler error or sanitizer failure. Native64-port runs
the same test on native ARM64/x64 and RV64/QEMU.

This does not prove instruction operand roles, canonical argument stores, a
prepared frame owner, definite assignment, boolean ranges, result lifetime or
return-value initialization. It does not decode an indirect LEA destination or
admit FRAME/CALL/RET/LEA in a file. FLASHVM-0063 accepts the initial
catalog/tail/acyclic subset choices, without allocating IDs or opening a loader;
the shared43-op np0 profile and ordinary Dis LEA remain unchanged. A future complete verifier must compose metadata, decoded
operand/setup state and control edges before any executable admission.
