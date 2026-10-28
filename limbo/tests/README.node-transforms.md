# Bounded AST copying, communication search and global sorting

Run `python3 limbo/tests/node-transforms.py` from the repository root.
The fixture extracts production `dupn`, `hascomm` and `gsort` with the real
compiler declarations. `NODE_TRANSFORM_NODES` and `NODE_TRANSFORM_TYPES`
select alternate nodes.c/typecheck.c; `NODE_TRANSFORM_CASE=small|copy|comm|sort`
selects an isolated family. `CC` and `CFLAGS` select compiler profiles.

Three further depth-proportional host stacks are removed:

- `dupn` retains preorder allocation: root, complete left subtree, complete right
  subtree. Pending entries hold borrowed source nodes and private destination
  cells. Shared DAG edges still receive independent copies. Every Node byte is
  copied, including borrowed Type/Decl metadata, with optional source-position
  replacement. The source-position pointer may refer to the original AST.
- `hascomm` returns the first Osnd/Orcv in root/left/right order. A matching root
  remains terminal even when its children contain unusable metadata. Ordinary
  misses and early hits release all private pending work.
- `gsort` descends only through Oseq, preserving left-before-right terminal
  processing. Each verified Omoddecl assigns the result of namesort before
  calling sizeids with that new head and offset zero. Nonsequence terminals
  remain opaque, including their child metadata and unverified modules.

All three use a fixed 64-entry work area and checked geometric heap growth.
Source AST nodes are unchanged. gsort deliberately updates verified Type.ids;
dupn allocates new compiler-owned nodes without acquiring metadata references.
Storage-size/allocation exhaustion remains fatal, as in adjacent compiler work.
Copying still requires a non-null source and acyclic ordinary child edges;
this change does not introduce a cycle-recovery or partial-copy rollback API.

The fixture compares 3,024 small copies byte-for-byte with an independent
recursive oracle, including allocation order, DAG expansion, nil replacement
source when disabled, external replacement source, and source-position aliasing.
It checks 1,008 exact communication results and 1,008 ordered sorting/sizing
callback logs, plus nil and matching-terminal nonaccess cases. Private traversal
allocation/free accounting remains active with NDEBUG.

Each of the three deep families exercises 100,000-level right chains, left
chains and combs at a 256 KiB C-stack limit. Combs contain 200,000 nodes and
force pending-work growth. Copy output is checked against analytically known
preorder indexes, communication checks cover misses and deep hits, and sorting
checks exact reverse-spine callback order. Inputs are verified unchanged where
required. Default O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan profiles pass, all
with sibling-call optimization disabled. LSan process inspection is disabled;
explicit work ownership checks and the selected sanitizer instrumentation remain.

Against f77c625, isolated copy/comm/sort probes each reproduce ASan stack overflow.
A compilation failure is not accepted as regression evidence. Adjacent query,
declaration-chain and identity tests pass. Native bootstrap regenerates the real
grammar and compiles all 17 Limbo C units. The default interpreter checkpoint
includes this fixture. Other parser/folder/code-generator traversals can still
limit whole-source depth; these local guarantees do not promise arbitrary source
nesting through the entire compiler pipeline.
