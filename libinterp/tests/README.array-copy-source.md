# Array-copy source preflight

arraycpy now rejects nil or dead source owners before entering protected copy
construction. copyarray checks the element descriptor, root identity/lifetime,
signed length, allocation geometry and nonempty data span before heaparray.
Nonempty positive-width data cannot be nil/H or wrap the native address range.
Empty and zero-width arrays retain their valid nil/H data representations.
arraycpy(H) still returns H; saturated source/root counts need no new retain.

The independent fixture extracts production constructors, copier, destructors and
VM error protection. Seventeen cases per WORD model check source/root lifetime,
nil/H descriptors and payloads, address wrap, invalid lengths, saturated counts
and valid empty/zero-width copies. Rejection must allocate nothing and preserve
source/root/Type images; success must produce an independent root and restore
all owners after destruction. All 34 cases pass ASan/UBSan.

Replaying the parent heap.c with ARRAY_COPY_MODE=1 fails the dead-owner check;
mode 4 reproduces a UBSan null source passed to memmove. Both pass on the fixed
source. HEAP_CLONE_SOURCE selects an alternate production heap.c for replay.
These checks validate bounded metadata, not arbitrary inaccessible pointers.

Run python3 libinterp/tests/array-copy-source.py. The driver is included in the
94-driver core suite. See README.array-view.md for narrowing/copy/share/release
checks using actual C and executable AMD64 code.

Integration with the concurrent Loader result-slot change also updates the
module-type and RISC-V runtime extractors to include production loaderword.
The RISC-V fixture now uses WORD for both the status pointer and its storage,
matching the production API rather than a truncated host int. Both fixtures
pass with the updated Loader; no JIT command count changes.
