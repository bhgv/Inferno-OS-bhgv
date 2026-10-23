# Exception analysis sets and handler contexts

Run `python3 limbo/tests/raises.py` from the repository root.

The fixture compiles the complete production Elist analysis and checkraises
against actual compiler Node, Type, Decl and Sym layouts. Its 2,035 checks cover
sequence tails, nested branches, case/alt/pick arms, protected blocks, typed
handler re-raises, wildcards, duplicate declarations, literal/call/module-member
raise expressions, diagnostic sets and every allocation failure in a handler
example. Two thousand deterministic ASTs carry independently constructed
bitset oracles for escaping exception declarations.

Analysis now preserves already collected raises when processing a final leaf,
branch, scope or protected statement. Nested case/alt/pick statements borrow
the nearest handler's qualifier set instead of erasing it. Re-raises return a
copy of that borrowed set, so independent branch results can be merged safely.
Set union consumes its inputs, frees duplicate nodes, and handler/caught/final
sets are released when their analysis lifetime ends. checkraises diagnostics
therefore see the complete escaped set without retaining per-function Elist
allocations. New nodes use allocmem and its ordinary fatal diagnostic path.

ASan and strict UBSan are enabled by default. Ownership is counted after every
analysis in addition to ASan; result sets must be finite and duplicate-free.
Allocator failures run in child processes and must exit through fatal. Sandbox
ASan versions can emit no-return bookkeeping warnings in these terminating
children; ordinary bounds and use-after-free checks remain enabled.
LeakSanitizer process inspection is disabled.

RAISES_SOURCE selects an old implementation; RAISES_CASE selects examples,
random, warnings or allocation. The old checker fails the independently
expected warning sets, including a sequence of two different raises.

This checks compiler diagnostics and temporary ownership, not runtime exception
execution or JIT unwinding. ASTs are bounded adapters after type checking rather
than a complete parser/typechecker run. Existing treatment of wildcard/string
qualifiers and unknown re-raises remains unchanged; no new interprocedural
exception inference is introduced.
