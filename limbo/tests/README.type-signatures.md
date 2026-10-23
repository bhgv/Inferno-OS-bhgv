# Type signature bounds and wire compatibility

Run `python3 limbo/tests/type-signatures.py`. The driver extracts the production
signature generator and cleanup traversal from `types.c`, uses the actual Limbo
structures and repository MD5/block implementation, and compares folded hashes
against Python hashlib. `SIGNATURE_TYPES` can select a previous source revision.

Capacity sweeps check guard bytes, failure propagation and recursion-ID cleanup
for scalar, tuple, varargs, exception, polymorphic, fixed-point and recursive ADT
signatures. Additional cases cover negative/overflowed positions, a ten-digit
recursion ID at the buffer boundary, cached signed signatures on LP64 and an
8192-byte field name that forces allocation retries. Successful serialization
must match the existing signature spelling exactly.

The fixture adapts allocation, formatting and diagnostics; it does not invoke the
complete compiler or module loader. The default uses fail-fast UBSan. Override
CFLAGS to add AddressSanitizer. The two-gigabyte allocation limit is checked in
production arithmetic but is not materialized by the fixture.
