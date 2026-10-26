# Numeric formatting into reused pointer temporaries

Limbo may coalesce reference temporaries across ADT/array and string expressions.
For example, `string table.values[i].integer` can emit `cvtlc 8(tmp), tmp`, where
tmp still retains the Value ADT that contains the source number. The destination
must therefore release a generic old heap owner, not presume a String layout.

`cvtwc`, `cvtlc` and `cvtfc` capture the complete input before allocation and only
inspect/reuse old storage when its exact heap Type is Tstring. A different old
owner remains retained until a replacement string is complete. Allocation failure
leaves the slot, original owner and embedded source untouched. Publication precedes
release; shared old owners keep their external reference. String-specific corrupt
length/capacity/refcount rejection remains unchanged.

AMD64 and ARM64 numeric formatting leaves compare the old owner's exact Type with
Tstring before loading any String field. A mismatch declines without writes or
FP-state changes, allowing the C path to allocate and release the generic owner.
The private emission bound is 80 words to include the full-width identity guard;
code sizing/publication still fails atomically on a short caller buffer.

`integer-format-native.py` and `real-format-native.py` include array-owned embedded
numeric sources, shared/unique old owners and injected allocation failure in both
WORD models. Existing production-C/native decimal, Rune/cache, alias and FP tests
remain active. The ARM64/AMD64 instruction-emulation matrices add a wrong-Type
profile and continue checking full images, same-value writes, boundaries and FP
state. `leaf64-runtime.py` and compiler-only fixtures supply the builtin identity
used by real native publication; no arbitrary Type definition becomes production
fallback logic.

This was found by the FlashVM/Dis IDE live document inspector's bounded table
summary in both interpreter and native modes. Its actual Limbo/Dis/raster test is
an additional integration regression, not a replacement for these owning fixtures.
