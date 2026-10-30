# Array initializer destination publication

An initialized array assignment must keep its old destination alive until all
initializer expressions have been evaluated. In particular:

```limbo
cell.points = array[] of {cell.points[0], Point(400, 300)};
```

The old Oarray emission allocated directly into the destination and then called
arraycom on that same destination. A field/alias read in the initializer could
therefore read the new, not-yet-initialized array. This was reproduced by FlashVM
on its native-cell runtime with the real owning source compiler and both
interpreter/JIT dispatch. No uninitialized contents need to be printed to observe
the failure: compare the resulting Point with its expected copied value.

The initialized path now uses a fresh typed temporary, initializes all elements,
then publishes to the requested destination. The old array remains reachable,
the new array has an ordinary compiler-managed temporary root, and releasing the
temporary follows the final move. No VM allocation/GC semantics, wire ABI or
native64 table policy change. Allocation without element initializers retains its
previous direct path.

utils/tests/array-initializer-alias.b is an actual-runtime oracle covering an ADT
field, a second ADT alias, scalar and reference arrays, and wildcard initialization.
Compile it with the matching compiler/cell target, with and without -O, run on the
corresponding actual VM, reject FAIL and require its final PASS line. Compiling
alone is not an execution check. FlashVM's temporary NC64 target is not evidence
of a full classic Inferno boot or of new Dis64 execution.

Independent verification on2026-10-30: the prior compiler fails the ADT-field
oracle on FlashVM's actual native-cell interpreter and JIT. The fixed compiler
passes all five fixture cases with and without -O on both dispatch paths (four
runs). Fresh owning native-limbo.py with its default strict UBSan flags passes
72 real-source Dis/AOT cases and its existing ABI32 layout/CLI checks. This does
not claim that the new alias fixture was run on a complete classic Inferno VM.
