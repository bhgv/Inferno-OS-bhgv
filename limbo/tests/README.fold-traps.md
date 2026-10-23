# Exceptions during expression folding

Run from the repository root:

```sh
python3 limbo/tests/fold-traps.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 limbo/tests/fold-traps.py
```

The fixture runs actual recursive efold/folds/foldvc code with the production
setsideeffect table. It compares the value or exact exception before and after
folding, using actual interpreter byte/int/big shifts, integer powers, word-list
head/tail and string slicing. Each WORD32/WORD64 model runs 5,223 cases.

Cases include zero constant bases with dynamically read shift counts; nested
left/right shifts, powers, list head/tail and slices inside multiplication or
bitwise-and by zero, remainder by one, exponentiation by zero, boolean absorbers
and same-tree subtraction/comparisons. Valid inputs must retain their values;
invalid shift counts, zero to a negative integer power, nil lists and invalid
slice ranges must retain their exceptions. Known literal shift-count warning
recovery remains unchanged; the new shift cases use variable count nodes.

Both the zero-base rewrite and missing effect-table entries reproduce exception
elision with the previous sources. NODES_SOURCE and OPTAB_SOURCE allow independent
old-source substitution. FOLD_TRAPS_CASE selects zero, shifts, powers, heads,
tails, lists or slices.

Bounded AST construction and unrelated compiler dependencies reuse the identity
fixture. Recursive evaluation uses explicit static VM-style operand contexts;
exception jumps therefore do not abandon C stack-backed operand slots. This also
avoids stale stack poison when the sandboxed ASan runtime cannot inspect stack
metadata during longjmp; stack instrumentation remains enabled.
List movement and slice allocation use static adapters, not a live heap
or reference counter. Homogeneous ASCII strings are compared through their
lengths; general string comparison has separate tests. These fixtures are not
a complete source-to-Dis compilation, VM boot or native JIT execution. The two
word models exercise the same preserved-evaluation property with the respective
interpreter widths; the compiler's int type remains its existing 32-bit type.
Default instrumentation is UBSan. Supplementary ASan runs disable LeakSanitizer
inspection because sandbox process metadata is unavailable. No VM instruction,
layout, JIT backend, cryptographic code or security functionality is changed.
