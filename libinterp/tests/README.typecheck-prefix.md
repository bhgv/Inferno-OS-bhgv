# TCMP backward Type-field bounds

The C interpreter and AMD64/ARM64 leaves reject owner identities at or below the
backward distance to Heap.t before loading either Type pointer. Previously a
small nonzero owner could underflow that address or load from address zero.
TCMP reads only the Type field; it does not validate arbitrary mapped memory,
retain owners, change GC colors or establish liveness of forged references.
The H source remains a no-op. Ordinary type mismatch retains the interpreter
error path; native decline leaves the complete VM image unchanged.

The C path uses bytewise field reads rather than forming a synthetic Heap pointer.
Native leaves use unsigned comparisons before their existing Type loads. No ABI,
opcode IDs or availability count changes. Scalar channel additions from cc51c63
remain a separate upstream change.

`typecheck-prefix.py` extracts the production C opcode and executes actual AMD64
RX leaves in WORD32/native-pointer and legacy WORD64 models under UBSan. It tests
both source and destination low boundaries, H and matching valid Types.
`leaf64-jit.py` with LEAF64_CASE=typecheck executes the complete existing Type
matrix plus each new low-boundary identity on AMD64/ARM64 emulation, checking
aliases, unaligned slots and unchanged VM images. ARM64 emulation is not physical
hardware execution. Run with an explicit writable TMPDIR; instruction emulation
also requires Unicorn/Capstone through the explicit test dependency path.
