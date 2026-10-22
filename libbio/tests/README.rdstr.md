# Buffered line allocation and delimiter regressions

Run `python3 libbio/tests/rdstr.py` from the repository root.

The fixture compiles the production `Brdstr` and `badd` bodies with real Biobuf
declarations. Its input stub supplies short reads; its allocator always moves
successful reallocations and tracks the exact outstanding buffer. The 25,731
cases cover buffer capacities 1 through 32, short reads of 1 through 5 bytes,
16 delimiter values spanning the full byte range, delimiter stripping, embedded
NULs, repeated lines, and EOF without a final delimiter.

Injected allocation failures must stop immediately, release accumulated output,
and leave no returned partial owner or nonzero line length. They do not rewind
input already consumed. Direct helper checks reject negative and overflowing
lengths before allocation or source access. Both normal signed-char and
`-funsigned-char` builds are supported.

Defaults use strict UBSan. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libbio/tests/rdstr.py
```

`BIO_RDSTR_SOURCE` selects an alternate production file for before/after tests.
The old implementation fails the new delimiter regression. The I/O and allocation
boundaries are stubs; this is not a filesystem integration test or an Inferno boot.
LeakSanitizer process inspection is disabled in the sandbox; ASan/UBSan remain
active and the fixture requires exact allocation balance.
