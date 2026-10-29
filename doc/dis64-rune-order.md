# Dis64 Rune32 comparison contract

This agreed semantic subset follows [FLASHVM-0003](dis64-flashvm-journal.md)
and the compatibility choice requested in [INFERNO-0004/0005](dis64-inferno-journal.md).
It does not assign registry IDs or enable a new loader, compiler or runtime ABI.

## Stored values and order

A non-nil string record contains count:u32 followed by exactly count BE u32
values. Every payload bit is preserved, including surrogate values, U+0000 and
values above U+10FFFF. No Unicode normalization, replacement or scalar filter
participates in encoding or comparison.

For comparison, each 32-bit value has its signed32 interpretation. Implementations
may compare the unsigned keys `bits ^ 0x80000000`; they must not depend on an
implementation-defined unsigned-to-int conversion. Thus:

```
0x80000000 < 0xffffffff < 0x00000000 < 0x7fffffff
```

Strings compare lexicographically. At the first unequal Rune, its signed order
is the string order. If one sequence is a prefix, the shorter sequence is less;
equal sequences compare equal. The sign of the comparison result is semantic;
the helper publishes normalized -1/0/1. Ordinary Unicode scalars retain their
usual numeric order. A compact byte representation, if used internally, treats
each byte as 0…255 when compared with Rune32.

Nil-string equals the empty string by content in the existing language contract.
Nil identity, ownership and pool reference representation remain distinct. The
record-level codec describes non-nil records only; a nil-reference wire ID is
not assigned here. A resolver may compare nil through an empty record after
validating the reference according to the eventual module specification.

## Record helper and lifetime

`include/dis-rune.h:disrunecompare` validates both complete record extents before
publishing an order. Invalid/truncated/trailing-byte records or a null output
return failure with the output unchanged. Inputs are borrowed immutable spans
valid for the call. Read-only input spans may alias and need not be aligned.
The output must be separate from the inputs. No allocation, VM heap access,
retention, callback or suspension occurs.

This helper can back a typed string-case comparator after the caller resolves
and validates pool IDs. It neither resolves IDs nor assigns native PCs. The
active production stringcmp is unchanged by this helper addition.

## Evidence and limits

`python3 libinterp/tests/dis-rune.py` compares the C helper with independent
Python signed-integer list ordering. Coverage includes all pairs in a
deterministic record corpus, high-bit Rune values, empty/prefix order, invalid
extents, atomic failure and unaligned/aliased input. Existing exact-byte,
capacity and indexed-read fixtures remain. O3 UBSan checks the same code.

These are record/helper checks, not acceptance of new-ABI GC, ref-result unwind,
full module loading, host32 execution or physical ARM64 execution. Existing FIR
UTF-8 source-string syntax is not widened implicitly by this wire contract.
