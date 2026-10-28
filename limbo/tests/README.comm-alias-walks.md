# Iterative communication rewrite and tuple alias queries

Run `python3 limbo/tests/comm-alias-walks.py`. The driver extracts production
`rewritecomm`, `tupaliased`, `tupsaliased` and their private continuation helpers.
A frozen recursive implementation supplies a shallow differential oracle. Nodes,
Types, declarations and returned Inst records use the real compiler headers.
Descriptor-independent allocation/evaluation/generation callbacks are controlled
fixtures so their order, arguments and optimizer metadata can be checked directly.

`rewritecomm` retains preorder callbacks and historical child-result publication.
Each continuation borrows an operand cell; enter continuations read that cell only
when reached, and completion continuations publish the captured subtree only after
both children return. This matters when an earlier callback changes a sibling or
incoming cell. Even initially nil child cells are deferred. Addressable sends,
receive assignment reuse and temporary allocation/evaluation/release still prune
children at the same point, emit the same LEA and preserve `m.offset`/`m.reg`.
Private storage grows with explicit size checks and is freed on completion.

Alias queries walk in the original left-before-right order and release private
storage on both no-match completion and early match. Nonmatching Oname nodes keep
their metadata children searchable. The outer tuple query stops on its first
matching declaration. AST/Decl data remain borrowed. No hash/cache reorders either
query; repeated searches can still have the original product-of-tree-sizes cost.

Per WORD configuration and profile:

| Family | Scenarios |
| --- | ---: |
| 2,000 mixed/shared shallow graphs × send/receive × addressability | 8,000 |
| Callback mutation: deferred nil/right cells and captured subtree returns | 12 |
| 100,000-level communication rewrite | 1 |
| 100,000-level single-name alias query, miss and early hit | 2 |
| 100,000-level tuple-name query, miss and early hit | 2 |
| Null/metadata/nil-declaration and independent communication fast-path checks | 10 |
| Total | 8,027 |

WORD32/WORD64 and `-O1`/`-O0` ASan+UBSan plus `-O2` UBSan, all with `-DNDEBUG`,
yield **48,162 scenarios**. Optimized pointer/integer cast diagnostics are errors.
Native pointers remain native in both WORD configurations. Deep families use a
512 KiB C stack and force private stack growth with deferred siblings, then check
cleanup and source links. Ordinary alias queries additionally compare complete
source snapshots. Rewrite comparisons cover output links, returned root,
callback sequence/arguments, temporary source propagation and every generated
optimizer field.

Mutation cases independently require:

- a nil right cell filled by a left callback is visited;
- replacing the incoming cell does not replace the captured subtree returned by
  the original recursive invocation;
- changing a pending right child replaces the earlier child value at visit time.

The independent fast-path cases check addressable/nonaddressable sends, temporary
receive release, eligible receive-assignment reuse and exclusion of a nil-name
assignment destination. Callback counts supplement the differential traces.

`COMM_WALK_SOURCE` selects another `limbo/com.c`; `ALIAS_WALK_SOURCE` selects
another `limbo/ecom.c`. `COMM_ALIAS_CASE=rewrite|alias|tuple` isolates deep probes;
`shallow`, `mutation` and `edges` select other families. All three deep probes
independently reproduce ASan stack overflow on parent revision `11e2772`.
Build failures are not counted as reproductions.

The core suite includes this driver. Adjacent checks rebuild native tools and all
17 Limbo units, run node-query, reference-sort and reference-use regressions, and
verify JIT readiness. Instruction encodings and JIT coverage are unchanged; full
VM boot and physical ARM64 execution are outside this fixture.
