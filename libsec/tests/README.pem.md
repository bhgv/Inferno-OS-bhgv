# PEM boundaries and chain ownership

Run `python3 libsec/tests/pem.py` from the repository root.

The fixture compiles production decodepem.c and the production base64 decoder.
Its 55 cases cover LF/CRLF, an EOF-terminated END line, exact boundary/type
matching, empty payloads, every truncation of a complete block, continuation
pointers and all four allocation failures in a two-block chain. Truncated
inputs have exact heap allocations so ASan can detect boundary overreads.
Payload and chain-node ownership is counted independently of LeakSanitizer.
The default build enables ASan and UBSan; unsupported sandbox process inspection
is disabled. PEM_SOURCE selects an alternative parser for regression checks.

The parser computes ceil(3*n/4) without an overflowing intermediate and rejects
sections exceeding the decoder's signed-int input limit. Empty sections allocate
one byte even on allocators that return nil for malloc(0). The size-limit branch
is reviewed arithmetically, not exercised with a multi-gigabyte allocation.
On success new_s points after the complete END line; on failure it is unchanged.
A chain-node allocation failure frees its unlinked payload and returns the
already decoded prefix, matching the API's existing partial-chain convention.

This preserves the decoder's permissive base64 policy. It does not validate
certificates, establish trust, boot Inferno or simulate its native allocator.
