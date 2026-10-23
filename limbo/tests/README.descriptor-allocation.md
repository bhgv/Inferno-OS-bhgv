# Descriptor constructor capacities and empty maps

Run from the repository root:

```sh
MAP_CASE=constructors python3 limbo/tests/descriptor-maps.py
MAP_CASE=constructor-bounds python3 limbo/tests/descriptor-maps.py
```

mkdesc and mktdesc now compute ceil(size/(8*IBY2WD)) using quotient/remainder,
without adding a rounding bias to a potentially maximum signed long. Negative
and oversized extents are diagnosed before allocation; mktdesc validates its
input before creating a declaration or publishing a cached descriptor.
A zero-byte map still owns one allocated byte, removing dependence on whether
the native allocator returns nil for malloc(0). Its serialized map length stays
zero. Empty descriptors retain ordinary interning and type-cache behavior.

The shared fixture deliberately disallows zero-sized allocations. It verifies
six capacity boundaries (including INT_MAX), empty-map interning and caching,
and a positive pointer descriptor/cache. Five invalid constructor cases on the
LP64 host must use the compiler fatal path. These add 13 cases per word model,
bringing the full map suite to 2,101 per model, or 4,202 total.

The pre-constructor-fix source fails the empty-map allocation assertion.
Maximum capacity arithmetic is tested directly without allocating a giant map.
The host is LP64: removal of native-32-bit rounding-bias overflow is additionally
supported by arithmetic inspection, not a native ILP32 execution claim.
See README.descriptor-maps.md for source selectors, sanitizer details and
integration limitations. Descriptor formats and cache identity remain unchanged.
