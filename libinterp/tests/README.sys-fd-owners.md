# Native descriptor ownership in Sys results

Run `python3 libinterp/tests/sys-fd-owners.py` from the repository root.
`CC` and `CFLAGS` select the host compiler and flags; UBSan is enabled by default.
`FD_OWNER_SOURCE` selects an alternate platform `inferno.c`.
`FD_OWNER_CASE=single`, `files`, `pipe` or `connection` selects a subset.

The fixture extracts the actual native FD declaration, constructor/finalizer,
open/create/fildes/pipe/dial/announce/listen wrappers, ownership helpers, runtime
protection, string constructors/conversion and generated Sys frames. Native I/O,
heap allocation, file-group references and scheduler transitions are bounded
adapters. No real files, pipes or network connections are opened.

There are 3,430 scenarios per configuration, or 13,720 executions across emulator
and kernel sources and WORD32/WORD64 models. Both use host emulator error-stack
declarations; WORD32 changes VM words while retaining native pointers. These are
not full emulator/kernel builds or target JIT tests.

Checks include every owner-allocation failure and one position beyond the final
allocation, native failures, descriptor zero, shared previous handles, nil and
short pipe arrays, untouched extra array slots, and UTF-8 directory conversion.
The listen input borrows the previous result's directory. Tests verify exact
native descriptor roles, close-once ownership, file-group/heap/type references,
allocation guards, scheduler transitions and balanced runtime error frames.
Close adapters deliberately overwrite the error string: cleanup must retain the
original allocation or native error. Explicit counters check all heap blocks and
native descriptors; LeakSanitizer inspection is unavailable in the sandbox.

`mkfd` consumes a descriptor on both success and failure. Multi-owner helpers
mark each transfer before calling it, keep partial state volatile across error
jumps, and close raw descriptors plus managed partial results on failure. Pipe
slots and connection fields are published together only after all allocations
succeed. A failed native connection call clears both handles and its earlier
path; allocation failures retain the previous multi-owner result. Single-owner
file calls retain their established nil-on-error replacement behavior. Native
pipe creation now follows the release/acquire boundary used by other native I/O.

The full matrix passes UBSan and exact resource/guard checks. ASan+UBSan passes
all successful/native-error paths with:

```
FD_OWNER_CASE='all normal' CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-fd-owners.py
```

This runs 78 scenarios per configuration without injected allocation errors.
ASan also passes the `single` and `files` subsets with allocation failures.
The full nested pipe/connection failure matrix is not claimed to pass ASan in
this sandbox. ASan warns that it cannot determine stack bounds for nonlocal
unwinding, then reports a stack underflow while initializing the legal local
`saved[ERRMAX]` in the next cleanup frame. Host disassembly places that buffer
inside its new frame; stale red-zone shadow remains after the abandoned frame.
Stack instrumentation is not disabled or manually unpoisoned to suppress this
report. Run full ASan fault coverage in an environment with working stack-bound
inspection before treating it as complete.

Parent-source substitution reproduces unowned native descriptors in `single`
and `files`, nil-array failure in `pipe`, and premature replacement in
`connection`. Adjacent Sys directory/I/O/stream and slice/tokenization checkpoint
regressions are run against the combined branch state.

## Earlier owner-slot and pipe alignment checkpoint

The current matrix exercises all eight return-slot offsets for open/create/fildes
and both return/array offsets for pipe, including nil, absent and wrapping pipe
buffers. It checks that the old owner is absent before its finalizer runs and
that both pipe ends are published before old owners are released. The full
matrix now passes 2,702 cases per platform/WORD configuration (10,808 total)
under UBSan. ASan/UBSan passes all normal paths and the files allocation-failure
subset. The nested pipe/connection ASan limitation above still applies. See
[the complete FP and FD checkpoint](README.full-fp-fd-slots.md).

## Unaligned connection result checkpoint

The current matrix also covers all eight byte offsets for dial/announce/listen
result tuples. It preserves tuple padding, observes replacement/clear publication
inside finalization, and checks allocation failures with shared old handles and
a listen input borrowing the old directory. Full UBSan coverage is 3,430 cases
per configuration; ASan/UBSan passes 866 normal-path and 288 files-failure cases
per configuration. See [the division/connection checkpoint](README.minus-one-connections.md).
