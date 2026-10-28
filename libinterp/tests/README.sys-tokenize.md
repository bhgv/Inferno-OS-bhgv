# Sys.tokenize delimiter and ownership regression tests

Run `python3 libinterp/tests/sys-tokenize.py` from the repository root.
`CC` and `CFLAGS` select the host compiler and flags; the default enables UBSan.
`TOKENIZE_SOURCE` selects an alternate `libinterp/runt.c` revision.
`TOKENIZE_CASE=nil`, `content` or `failure` selects a subset.

The suite extracts production delimiter matching and token construction,
`cons`, `newstring`, `newrunes`, `slicer` and mapped pointer release (`freeptrs`).
It uses actual Sys frame, String, List and Heap declarations. Each run extracts
`kprotect` and `krethrow` from either the emulator or kernel runtime. Both are
bound to host emulator error frames, not a booted kernel. The allocator and
final destruction are bounded ownership adapters: payloads are poisoned, every
allocation has exact guards, and all live blocks and type references are counted.

There are 83,284 scenarios per runtime/WORD configuration, or 333,136 executions
across emulator/kernel protection implementations and WORD32/WORD64 models.
The WORD32 model changes VM word declarations on a host with native pointers;
it does not emulate a 32-bit pointer ABI. No full emulator, kernel or JIT run is
claimed. LeakSanitizer process inspection is unavailable; explicit live-block
checks verify that every fixture allocation is released. ASan/UBSan can both be
used without disabling stack instrumentation.

An independent character-array oracle checks word contents, order and count.
Cases include nil and empty source strings, nil and empty delimiters, ASCII and
Rune representations, embedded NULs as data and separators, Unicode including
supplementary-plane characters, maximal delimiter runs and source/delimiter
aliasing. Output words must be copies and input strings must remain unchanged.
The old result owns a reference to the input, exercising replacement ownership.

`nil` delimiters specify no separators, as documented in `man/2/sys-tokenize`:
a nonempty source produces one copied word. Pointer-bearing list payloads must
use `sizeof(String*)`, independently of the bytecode's four-byte word constant.
The suite injects failure into each list/string allocation of a five-word result
and just beyond the final allocation. Each failure must rethrow the original
error, release partial words and nodes, and preserve both fields of the caller's
old result. Poisoned list heads ensure cleanup cannot assume unused memory is nil.

Parent-source substitution independently reproduces incorrect nil-delimiter
results, overwritten allocation guards from four-byte pointer payloads, and
leaked partial lists on allocation failure. Existing list, string, codec, Sys UTF
and Sys print suites provide adjacent regression coverage.

[Token frame and delimiter spans](README.token-spans.md) add packed borrowed
delimiters, complete generated frames and return tuples, old-owner preflight
and allocation-failure cleanup with active ASan/UBSan release-mode checks.
