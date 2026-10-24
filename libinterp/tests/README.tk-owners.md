# Tk result and channel ownership

Run `python3 libinterp/tests/tk-owners.py` from the repository root.
`CC` and `CFLAGS` select the host compiler and flags; UBSan is enabled by default.
`TK_OWNER_SOURCE` selects an alternate `libinterp/tk.c` revision.
`TK_OWNER_CASE=quote`, `bounds`, `top` or `name` selects a subset.

The fixture extracts complete production `Tk_toplevel`, `Tk_namechan`, `Tk_quote`,
string constructors/conversion, `retstr` and runtime protection. Actual Tk,
interpreter and generated builtin-frame records supply their native layouts.
Allocation/destruction, variable lookup, context locks and existing-program
lookup use bounded adapters. New-top/window/context construction stubs abort:
only reuse of existing tops is exercised, not GUI creation or rendering.
The destruction adapter models the program's weak top pointer and clears it on
final release, as the real top finalizer does. Pool mutability retains its native
GC-color behavior.

Each WORD64/WORD32 model executes 14,223 scenarios, or 28,446 executions total.
WORD32 changes VM word declarations while retaining native host pointers; these
are not two native pointer ABIs or target JIT tests. The full matrix passes both
UBSan and ASan+UBSan with stack instrumentation enabled:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/tk-owners.py
```

Explicit block/type-reference accounting and exact allocation guards cover all
fixture heap owners. LeakSanitizer process inspection is unavailable. The
sandbox emits its stack-bound warning for error jumps, but this fixture's complete
matrix passes without suppressing reports or unpoisoning stack memory.

Existing-top cases reuse the same result up to 32 times with nil, unrelated and
sole-alias previous owners. Every call must retain a new result before releasing
the old owner; dropping the final result must clear the program's weak pointer.

Channel-name cases cover distinct/same channels, variable allocation/type errors,
and deterministic replacement/deletion immediately on context unlock. The unlock
adapter can free the variable record, so no post-unlock access is safe. Tests
check references, the channel GC mark, error replies and balanced locks. This
models an interleaving; it is not a real multithreaded Tk stress run.

Quote cases compare character escaping with an independent Rune-array oracle,
including both representations, Unicode/supplementary characters, embedded NULs,
empty/nil strings, braces and backslashes. Previous results can be nil, unrelated
or the input's alias with a separate owned reference. Each output allocation is
also failed, requiring the old result and input owners to remain valid. Widened
length accounting uses the actual ASCII/Rune constructor bounds before narrowing.
Extreme metadata cases reject oversized results before reading bytes or allocating;
no multi-gigabyte test allocation is attempted.

Parent substitution reproduces lost top references, unsafe variable access after
unlock, discarded quote results on allocation failure and signed overflow in
`INT_MAX+2`. The host Tk translation unit passes syntax checking. Tk autorepeat,
Prefab selection, GUI map and shared host-checkpoint tests provide adjacent
coverage. No full Tk/Inferno build, native event loop or JIT execution is claimed.
