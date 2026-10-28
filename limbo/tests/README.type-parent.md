# Parent-Type compatibility and lazy reconstruction without native recursion

Run `python3 limbo/tests/type-parent.py` from the repository root.
`TYPE_PARENT_CASE` selects shallow, check, build, tuple, identity or nil.
`TYPE_PARENT_TYPES` selects alternate production types.c for isolated replay.

## Changes

`tparent` first checks whether two Types admit parent generalization and then
builds the result. Both passes previously recursed through wrapper and tuple
edges. The second pass also traversed an entire Type when both arguments were
the same identity. Consequently deep valid structures exhausted the compiler's
native stack even when no Type would change. Identical nil arguments passed the
predicate and then reached a nil dereference in the builder.

The public function now returns identical arguments immediately, including nil.
The compatibility pass traverses wrappers iteratively and keeps deferred tuple
fields in a checked private stack. It retains field order and checks tuple length
after the common prefix, so an incompatible field still short-circuits before
later fields are inspected. Nominal Types and picked parents continue to use
the existing tcompat/tequal delegates.

The result builder uses explicit continuations for wrapper returns and tuple
fields. It retains the original copy-on-change behavior: unchanged wrappers and
whole tuples are borrowed directly; the first changed tuple field triggers one
mktype/dupdecls operation, and later field updates reuse that copy. Source metadata
and copied declaration metadata remain intact. Normalization between a pick and
its parent ADT preserves the existing result identities.

Both work stacks use small inline storage and checked geometric heap growth.
Every successful, unchanged or rejected traversal releases its private stack.
Newly constructed Type/Decl graphs retain the compiler allocator's ordinary
ownership; they are not temporary work blocks. Fatal allocation failures keep
the existing compiler process-exit behavior rather than introducing rollback.

This removes recursion in these structural parent traversals. It does not replace
recursion inside the nominal compatibility/equality delegates, add cycle detection
for illegal structural cycles, or cache repeated shared subgraphs.

## Evidence

The fixture extracts the real parent traversals, mktype, dupdecl and dupdecls.
A test-only copy of the historical recursive rules supplies shallow result and
allocation oracles. Nominal equality/compatibility bodies use deterministic
family/tag fixtures and record nonidentity delegate calls; this isolates parent
traversal order rather than claiming a full replacement test for those bodies.

| Family | Scenarios per profile |
| --- | ---: |
| 3,000 shallow pairs and three explicit order/copy cases | 3,003 |
| 100,000-wrapper predicate, reuse, mismatch and identity | 4 |
| 100,000-wrapper construction, reuse, mismatch and identity | 4 |
| 20,000-level tuples through both passes | 8 |
| Deep identical Type identity | 1 |
| Identical nil and one-sided nil arguments | 3 |
| Total | 3,023 |

Shallow checks compare complete result shape, exact borrowed Type identities,
constructor allocation counts, copied declaration metadata, delegate order and
unchanged Type/Decl snapshots. Cases cover all simple scalar kinds, wrapper kinds,
nominal delegate categories, parent/pick normalization, unequal tuple lengths,
shared identities and changes after unchanged tuple prefixes. An incompatible
field and a late length mismatch are tested with unread invalid later field
identities to require the original short-circuit behavior.

Deep cases use a 512 KiB native stack and independent result-chain checks.
Changed wrappers create exactly one Type per level. Changed tuples create one
Type and two declarations per level through the actual constructors. Full
unchanged reconstruction and deep rejected predicates retain the original root
and leave no constructor or temporary owners. Identical root queries allocate
neither temporary storage nor result nodes.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass under NDEBUG with sibling-call
optimization disabled: 9,069 scenarios. The optimized profile treats native
pointer/int conversion diagnostics as errors. LSan process inspection is disabled;
address instrumentation and explicit owner accounting remain active.

Against parent 50059a3, separate check/build/identity probes reproduce ASan
stack-overflow, and the nil probe reports a UBSan null Type member access.
Compile failures are not counted as runtime regression evidence. The old source
also passes the 3,003 shallow semantic/order cases. Adjacent descriptor mapping,
zero-initialization queries and native bootstrap pass; bootstrap builds ELF64
host tools, regenerates the real parser/decoder and compiles all 17 Limbo units.
The new driver is registered in the core compiler/interpreter suite.

No native opcode encoders change here. AMD64/ARM64 readiness remains 173/173
complete JIT+C entries, 166/173 guarded native fast paths and seven C-only entries.
This does not establish a complete booted 64-bit VM ABI. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources remain unchanged.
