# Exception-owner and message lifetime preflight

IRAISE now rejects dead and saturated exception-owner reference counts before
following tuple descriptors, terminating ASCII text or allocating UTF-8 caches.
After resolving a tuple message it also requires a live String owner, unless
the message is H (the existing empty-message convention) or nil (the existing
nil-reference error). A different heap Type is rejected before interpreting
its payload as a String.

The exception retain and process exval publication remain after successful
message preparation. The last legal retain, from native unsigned MAX-1 to MAX,
is valid. A saturated borrowed message is also valid because its count is not
incremented separately. Failure preserves owner counters, message header/data,
old cache and process exval. No new descriptor or collector retain is added.

raise-slots.py extracts the actual IRAISE and string2c bodies. Its fixture now
uses explicit checks and documented functions/defines. Across both WORD models,
8192 cases exercise all eight source alignments, tuple/direct ASCII/Rune
messages, four source and four message counter profiles, poisoned tuple
descriptors, wrong message Types, nil/H messages and forced cache failure.
Old terminator/cache bytes make preparation-before-preflight visible. Both
models pass ASan/UBSan.

RAISE_SLOT_SOURCE selects an alternate interpreter source; RAISE_MODE and
RAISE_COUNTER select independent regressions. Parent replay reproduces wrapped
exception retention, acceptance of a wrong-Type message and a UBSan poisoned
Type dereference before checking a dead owner. These are lifetime/type checks
for readable metadata, not an arbitrary pointer sandbox.

This belongs to the same 98-driver checkpoint as README.list-length.md. It
adds no native opcode ID or private ABI field; exception dispatch remains C.
