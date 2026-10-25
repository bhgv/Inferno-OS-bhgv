# Conversion source lifetime and native byte spans

CVTCA now rejects a dead source string before scanning its ASCII/Rune payload
or allocating a byte array. CVTAC validates the source array reference, root
identity/lifetime and nonempty native byte range before c2string. A dead array
is rejected before following its borrowed element descriptor, including when
that descriptor address is poisoned. Its existing
byte-descriptor check still accepts compiler-owned pointer-free size-one Types.
The public c2string helper independently rejects an overflowing end pointer
before UTF scanning, covering native library callers as well as VM operands.

Empty arrays require a live valid owner/root but no payload access: nil/H data
and unused near-maximum addresses remain valid for zero length. H source
semantics are unchanged. Nonempty nil/H payloads retain nil-reference errors.
Saturated source/root counts are accepted because conversions borrow them; no
source retain is acquired. Constructor allocation/descriptor failures preserve
the old destination. These are bounded metadata checks, not validation of every
arbitrary inaccessible pointer.

conversion-sources.py extracts the production conversion/constructor bodies and
links real rune.c. Its new fixture uses explicit checks at all eight input and
output byte alignments, snapshots sources/roots/heap counters and surrounding
slot bytes, and checks final ownership cleanup. Per WORD model, 2688 cases
cover 24 array-source profiles and 18 string-source profiles. Eleven independent
public-helper probes additionally cover wrap, nil/H, negative/oversized/empty
lengths and ordinary payloads. All 5398 cases pass ASan/UBSan.

STRING_SOURCE selects an alternate production string.c. CONVERSION_MODE selects
0..23 (CVTAC), 100..117 (CVTCA), or 200 (public c2string). Parent replay fails
for dead array/string acceptance and reproduces a UBSan pointer-index overflow
for the wrapping byte source. The fixed helper rejects before allocation.
The shared legacy codec fixture now gives its forged headers live owner counts
and explicit root identity; its prior nil/type/length error expectations remain
unchanged. Existing codec and unaligned-slot matrices pass.

Run python3 libinterp/tests/conversion-sources.py. The driver is included in the
96-driver core suite. This series also extends exact-capacity ARM64/AMD64 integer
formatting; see README.integer-format.md. No Dis opcode IDs are added here.
