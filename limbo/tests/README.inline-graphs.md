# Bounded inline dependency graphs and cycle refusal

Run `python3 limbo/tests/inline-graphs.py` from the repository root.
`INLINE_GRAPH_SOURCE` selects another com.c; `INLINE_GRAPH_CASE` selects small,
tree, chain or cycle. CC/CFLAGS select the compiler and instrumentation.

The previous ckinline0 recursively traversed both AST children and the bodies
of declarations marked caninline=1. Besides host stack exhaustion, a cycle
between candidate functions unrelated to the declaration currently being checked
could recurse forever: the old termination condition recognized only that one
declaration, not active intermediate candidates.

The replacement uses a fixed 64-task work area with checked geometric heap
growth. Ordinary nodes retain left-before-right traversal. Oname remains terminal
unless it names a candidate function; its unrelated child metadata is ignored.
Each expanded candidate temporarily receives the private signed-char value -2.
A restoration barrier clears that mark after its body finishes. Encountering an
active candidate rejects inlining, as does encountering the original declaration.
Both successful completion and every ordinary early rejection restore all pending
marks and free private work. Stable compiler states remain -1, 0 and 1. No AST
nodes, declaration references or function bodies are rewritten by this check.

This is a single-threaded internal compiler traversal. Allocator/storage exhaustion
is fatal, consistent with adjacent compiler passes; no recoverable fatal-state
rollback is promised. Ordinary AST child edges must remain acyclic. Shared inline
dependencies may be revisited after their restoration barrier, preserving the
previous per-edge meaning rather than introducing a persistent memoization cache.

Small tests compare 2,304 acyclic graph results with the original recursive rules,
check nil and disabled candidates, and exercise shared transitive dependencies.
Name child fields include unusable metadata pointers to enforce terminal behavior.
Each comparison checks complete declaration/AST images and private work ownership.

Each deep family runs three cases with 100,000 levels under a 256 KiB C-stack
limit: right/left/comb ASTs, transitive candidate chains with success or a deep
reference to the checked declaration, and self/two-function/long active-path
cycles not containing the checked declaration. All declaration flags are checked
for exact restoration, including early refusal with pending work.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass, all with NDEBUG and sibling-call
optimization disabled. LSan process inspection is disabled; explicit allocation
accounting and selected sanitizer instrumentation remain active. Against 2bd9f3c,
the isolated tree, chain and unrelated-cycle probes all reproduce ASan stack
overflow. Compilation failures are not accepted as regression evidence.

Native bootstrap regenerates the grammar and compiles all 17 Limbo units.
This fixture is registered in the default interpreter checkpoint. Other recursive
compiler passes and native VM ABI integration remain separate acceptance work.
