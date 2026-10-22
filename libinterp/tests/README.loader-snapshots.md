# Dynamic Loader snapshots

Run `python3 libinterp/tests/loader-snapshots.py` from the repository root.

The fixture executes production Loader_ifetch, Loader_link, Loader_tdesc,
branch conversion, heaparray, nheap, array initialization, freeptrs, mem2array
and emulator kprotect bodies. Each of two WORD models runs 231 cases. Native
pointers retain the host width in both models; word32 is a scalar-width test,
not a complete 32-bit ABI port.

Coverage includes returned instruction fields, all 33 branch/spawn/call opcodes
with four targets, reversed export order, exact signatures/type indices,
builtin export markers, empty arrays, descriptor maps, repeated replacement
of live results and invalid counts before allocation. Every allocation in
all three queries fails independently and repeats eight times; array, string,
byte-array and Type references must return to their initial counts. Compiled
native-PC queries are rejected, while absent frame membership is bounded by
ntype instead of searching past the type table.

A bounded pool allocates exact requested payloads plus verified guards. String
construction and final destruction are injected boundaries; destruction walks
the actual production pointer maps through freeptrs. The real heaparray
initializer must set every pointer to H before any child allocation can fail.
Original bodies independently fail partial ownership, branch conversion,
compiled metadata and count-boundary families.

Strict UBSan is the default. ASan also passes with stack instrumentation enabled:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/loader-snapshots.py
```

Only unavailable LeakSanitizer process inspection is disabled.
`LOADER_SNAPSHOT_SOURCE` supplies an alternate loader.c.
`LOADER_SNAPSHOT_CASES=values|failures|replace|branches|metadata|bounds` isolates
failure families. This does not boot Inferno, run the real GC, execute native
compiled code, or validate concurrent modification of source modules.
