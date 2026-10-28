# Bounded compiler AST queries

Run `python3 limbo/tests/node-queries.py` from the repository root. This driver
extracts the production `hasside`, `hascall`, `hasasgns`, `nodes` and `sametree`
functions and uses the real compiler Node/Type/Decl/Sym declarations.

All five traversals now use a fixed 64-pointer work area and checked geometric
heap growth instead of depth-proportional C recursion. Pending right branches
are processed after left branches. Predicates preserve their original operator
rules, including strict/non-strict Oadr/Oind handling, call/spawn recognition,
and the Ocall/Onothing exceptions in `hasasgns`. `nodes` counts repeated DAG
edges independently and rejects signed count exhaustion. Allocation and size
exhaustion remain compiler-fatal resource errors, matching `occurs`.

`sametree` stores pending pairs. Pointer identity remains immediately equal;
constants remain terminal; Type and Decl identities, string Sym identity,
fixed-point equality and floating-point comparison retain their previous rules.
Distinct NaN constants remain unequal, an identical NaN node remains equal, and
positive/negative zero compare equal. A left mismatch does not dereference a
pending invalid right pointer. Inputs and declaration references are unchanged.
All private traversal memory is released on ordinary success and early exits.

The fixture checks 20,480 small-tree query results against recursive semantic
oracles, including nil, mixed operators, table variations and shared subtrees.
Additional equality cases cover every supported constant category, unsupported
constant categories, cyclic constant-child metadata, types, declarations and
IEEE behavior. Deep runs lower the C stack limit to 256 KiB and exercise:

- 15 complete query traversals of 100,000-node chains or 200,000-node combs;
- 12 deep predicate hits, including cleanup after work-buffer growth;
- three equal deep tree pairs and three deep comparison mismatches.

Default O1 ASan/UBSan/NDEBUG checks pass. O0 ASan/UBSan and O2 UBSan runs also
pass, with sibling-call optimization disabled in all three profiles. Private
allocation/free accounting remains active under NDEBUG. LSan process inspection
is disabled in this environment; stack/address/undefined instrumentation stays
active where selected. These are local traversal guarantees; other recursive
parser, folder and code-generation passes can still bound accepted source depth.

Against `b151e32`, separate `NODE_QUERY_MODE=0|2|3|4|99` deep probes reproduce
ASan stack overflow in hasside, hascall, hasasgns, nodes and sametree respectively.
`NODE_QUERY_CASE=deep` selects those probes; `NODE_QUERY_SOURCE` selects the old
nodes.c. Mode 99 skips the four query families and reaches equality testing.
Compilation failures are not counted as reproduced bugs.

Adjacent identity/trap/lazy/result-type/string-length folding, prior node-chain
checks and native bootstrap (real grammar and all 17 Limbo units) also pass.
The standard interpreter checkpoint runner now includes this fixture.
