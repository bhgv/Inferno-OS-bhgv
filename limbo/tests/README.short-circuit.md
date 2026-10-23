# Lazy constant folding and inline control flow

Run `python3 limbo/tests/fold-short-circuit.py` from the repository root.
The default build uses UBSan. Set `CFLAGS` to select other compiler flags;
`CC` selects the C compiler. No installed Inferno toolchain is needed.

The fixture extracts production `efold` and its folding dependencies from
`limbo/nodes.c`, `rewritecontrol` from `limbo/ecom.c`, and arithmetic operations
from `libinterp/xec.c`. Bounded AST storage, diagnostic counters and a small
expression evaluator bind these functions. Unrelated rewriting and inlining
use a controlled traversal adapter with copied, already-substituted inline
bodies; this is not a complete compiler, type checker or VM execution.

Each WORD32/WORD64 build checks 4,620 cases across three paths: folding alone,
rewrite followed by folding, and rewriting with nested inline calls. Cases cover
`&&`, `||` and the compiler-internal `Oif` expression produced by inline return
branches. Conditions are literal, derived or unknown variables. Arithmetic
includes byte/int/big division and remainder by zero, zero raised to a negative
power, and invalid left/right shift counts. Invalid selected expressions retain
the compiler's diagnostics, including shift warnings and recovery. Invalid
expressions under unknown conditions still receive diagnostics. Known discarded
expressions must remain untouched and must not be inlined or diagnosed.

Valid logical expressions check normalized boolean values and the exact number
of variable evaluations. Selected inline return values retain their original
value type. Type checking occurs before these transformations and is unchanged.
The WORD64 fixture is an arithmetic width model, not evidence that a native
64-bit JIT backend exists or has been tested.

`SHORT_CIRCUIT_CASE=and`, `or`, `inline` or `valid` selects one group.
`NODES_SOURCE` and `ECOM_SOURCE` can point to older compiler source files.
Substituting the parent revision's nodes file reproduces eager dead-arm
constant diagnostics. Substituting its ecom file exercises the adapter's legacy
eager traversal and reproduces premature rewriting of discarded inline calls;
that comparison models the old traversal rather than extracting all of rewrite.
