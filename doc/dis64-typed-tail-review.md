# Inferno response to FLASHVM-0037: internal typed variadic tails

Inferno accepts the six principles in FLASHVM-0037 for a trusted in-process
prototype: explicit ordered types/extents, eight-byte slot alignment, complete
dynamic roots, caller frame ownership, borrowed callee arguments and protected
result handoff. This is a design response, not implemented variadic-call support.
The following exact representations are proposed for peer confirmation. No wire
ID, serialized layout, C va_list reinterpretation or file admission is assigned.

## Proposed canonical slots

Each tail slot is8 bytes, aligned to8. The numeric slot is stored in native byte
order for this internal adapter only; it makes no proposal about file byte order.
The vector states count, kind, checked slot extent and expected trusted reference
Type identity. Any kind values remain private implementation details.

| Kind | Canonical eight-byte value | Root bit |
|---|---|---:|
| byte | Zero-extended unsigned8; upper56 bits zero |0|
| int/fix | Sign-extended signed32; preserve fix's raw scaled integer |0|
| big | Signed64 two's-complement bits |0|
| real | Exact IEEE binary64 bits, including signed zero and NaN payload |0|
| managed reference | Native64 VM object pointer, or H for the empty value |1|

Thus int(-1) and the empty reference may have the same bits but distinct kinds;
only the latter is mapped. Nil0 is rejected as a managed-reference value, without
silent nil-to-H conversion. Raw C buffers and foreign pointers are not managed
reference slots. A future exception needs a separate explicit policy.

The tail begins at the aligned end of the agreed fixed prefix. Its vector cannot
mark Frame64 links, result-address or reserved bytes. The complete map combines
validated fixed-argument roots and typed tail roots, with checked count/extent/
addition/multiplication and zero padding bits. Format parsing may validate how
values are displayed; it never determines argument ownership or root layout.

## Descriptor ownership and preflight

The protected call context owns immutable vector/map storage and retains the
dynamic Type until frame retirement. Initially this is stable owned C storage,
not a naked pointer into movable VM memory. If any descriptor component becomes
a managed object, the context must register its owner as an explicit GC root.
Keeping a C pointer alone would be insufficient.

Preflight has distinct phases. First validate descriptor/vector/count/extent
without reading the argument frame, allocating or producing formatter output.
Then validate supplied values against the checked descriptor while their existing
caller owners remain rooted. Trusted-readable VM value provenance is a precondition;
this is not a way to authenticate arbitrary machine addresses. Reject nil policy,
noncanonical scalar bits and wrong trusted reference identities before invoking C.

Allocate the new frame with all mapped fields initialized to H before publishing
it as a root. Retain its descriptor/context and register that root before filling
managed slots. Copy/retain arguments under the existing root/barrier discipline;
errors retire only acquired owners. Nested allocation/callbacks must see complete
roots for input values, frame, descriptor and any result-under-construction.

## Result handoff and required evidence

The caller owns the frame until retirement; C borrows arguments. A produced
reference result first belongs to the protected call context. Keep the old caller
result rooted until a checked commit resolves the current destination, publishes
the acquired new owner and retires the old owner. A raw destination address must
not be reused across operations that may move its owning object. Before commit,
unwind preserves the caller's old destination and releases only new acquisitions.
This follows the existing protected-result discipline rather than inventing an
unrooted shortcut for the formatter.

Please confirm sign extension for int/fix, exact real bits, H-only empty managed
references, context-owned descriptor storage and this staged preflight/handoff.
The prototype still requires actual constructor/GC/unwind tests with mixed values,
wide maps, malformed extents, moved result owners, nested allocation and exception/
cancellation cleanup. Geometry-only tests are insufficient. Sys's four variadic
refusals and all current Dis64/NC64 admission limits remain in force meanwhile.
