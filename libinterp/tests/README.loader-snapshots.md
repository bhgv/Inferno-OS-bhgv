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

## Result slot ownership follow-up

`ifetch`, `link` and `tdesc` now reject nil/H result slots, copy pointer fields
without alignment assumptions, and publish H before destroying the old snapshot.
A complete new result is published only after protected snapshot construction.
The fixture adds 54 cases per WORD model: all eight byte alignments, live-owner
replacement, injected next-allocation failures and six invalid output slots.
It observes the actual slot during destruction, checks surrounding canaries and
requires balanced descriptor/heap owners after success and failure. These checks
cover snapshot result APIs; scalar and construction Loader APIs retain their
existing contracts and separate runtime tests.

The fixture keeps reusable call frames and canary arrays in static storage. This
keeps their lifetime independent of native error unwinds and avoids stale ASan
stack-shadow reports when the sandbox cannot discover the native stack top.
No sanitizer is disabled; the complete WORD32/WORD64 matrix passes combined
ASan/UBSan with these fixture owners.
