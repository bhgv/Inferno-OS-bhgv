# Proposal: first bounded scalar local-call profile

Status: initial subset design ACCEPTED by FLASHVM-0063 on2026-11-02; executable
admission remains closed. This design builds on
the agreed scalar signature/export codecs in dis64-storage.md and the narrow
LEA/result-address rule in dis64-contract.md (INFERNO-0070 / FLASHVM-0061).
It allocates no section, opcode or feature ID. Current readers must continue to
refuse file FRAME/CALL/RET/LEA until activation and executable goldens are agreed.

## Reuse existing metadata first

Use the existing exports records as the complete callable function catalog:
name ID, entry PC, frame Type ID and exact version1 scalar signature. Initially
require one record per entry PC, including the module entry with void result
and zero parameters. The module entry PC/frame Type must match its catalog
record. Sort the checked entry PCs to define disjoint instruction
intervals; the entry interval begins at PC0. A normal branch/case target must
remain in its own function. Only CALL crosses intervals, at an exact declared
entry, with the matching frame Type/signature. Unknown gaps, aliases at one PC,
overlapping ownership and malformed signatures refuse before allocation.

This deliberately makes all initial local functions declared exports rather than
inventing another function section. It does not enable imports, MCALL, native
module loading or external invocation. Keep wire exception tables and managed/
concurrent operations closed in this first subset too. Existing codec acceptance is not enough:
the executable profile imposes these additional ownership checks.

## Initial subset and frame geometry

Keep int32/bool4, big/real8 parameters/results and np0 MP/frame maps. Canonical
parameter layout starts at byte64, aligns each field to its scalar width and
uses the full field span. No managed/aggregate/variadic signatures initially.
Bool arguments/results must remain0/1; equal width alone is not an int/bool
compatibility proof. Frame size is aligned8 and covers arguments, private
numeric locals and the private caller result cell described below. Numeric locals must be definitely
assigned before use; this proposal does not silently change ordinary FRAME
initialization semantics.

Initially permit one prepared outgoing frame per active function. Evaluate
argument expressions before FRAME, then checked argument stores, optional result
binding and CALL. Reject branch/case entry into the setup interior; merged CFG
states must agree on the same prepared owner/type and initialized arguments.
Reserve native pointer temporaries only in the agreed prefix slots40/48/56;
ordinary numeric instructions cannot overwrite these pointer cells. General
pointer copies, indirect memory and escaped frame pointers remain closed.
A finite acyclic call graph and explicit code/frame/depth/resource limits are
the agreed initial subset policy, not new wire integer limits.

## Initial private result cell

Reserve the final aligned eight bytes of EACH caller frame exclusively as its
outgoing scalar result cell. Require it to lie fully after the complete argument
span; compiler-generated numeric locals must also exclude it. Its value kind is
bound to the checked callee signature at each call site. This gives the initial
verifier a concrete result-storage provenance without another local-type table.
No ordinary read/write may treat a pending/uninitialized result as a value.
Preparing another call invalidates the old cell value. Copy a needed prior
result to a definitely assigned numeric local before reusing the cell. Failure
or unwind cannot publish a partial result. Exact signature kind, not just width,
controls access: a four-byte result cannot use an eight-byte store.

For a non-void call, LEA derives that caller-owned cell address at runtime and
writes the complete eight-byte address ONLY to the prepared callee's byte32
result-address cell. The callee may store the exact declared scalar width only
through its own incoming result-address cell, with zero secondary offset.
Every returning path must establish the declared result before RET. After CALL
returns, the caller may read only the declared result width. Void calls need no
result binding. The interior address is not a GC reference or serialized pointer.
Caller storage stays live through success and unwind. No ref-result ownership
is inferred; managed results remain a later independent profile.

Ordinary Dis LEA must retain its old WORD behavior. The future file-profile
implementation needs an explicit checked execution mode so its eight-byte
address operation and native JIT cache cannot accidentally reuse a four-byte
numeric LEA entry. This is a runtime implementation requirement, not a new ID.

## Review decisions — FLASHVM-0063

1. Accepted: reuse exports with unique entry PCs and disjoint function ownership.
   This does not require future private functions to remain exported, and exports
   alone grant no external/native invocation capability.
2. Accepted: a dedicated final aligned eight-byte result cell, with signature
   kind, full-span non-overlap, definite assignment and pending-state rules.
3. Accepted: one prepared outgoing frame and a finite acyclic bounded call graph.
   CFG merges must agree on owner/type/initialized arguments/result binding; all
   return, failure and abandon paths retire ownership exactly once. Verifier
   resource caps must be explicit. General preparation/recursion comes later.

These decisions allocate no activation IDs and do not open the loader. A precise
verifier state model, matching C/RX execution and whole-file
positive/refusal goldens must precede admission. Initial scalar execution should
be proven on native ARM64/x64 and RV64/QEMU before widening the subset.
