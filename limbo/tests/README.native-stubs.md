# Native stub names and fixed map capacity

Run `python3 limbo/tests/native-stubs.py`. It extracts the production lowercase,
modcode and modtab generators and uses actual compiler declarations, types and
descriptors. Frame layout, type signing and compiler-specific output conversion
are explicit fixture adapters; this test does not invoke the entire frontend or
claim to verify argument offsets for a target ABI.

Dynamic and static code generation cover module-name lengths 0, 1, 31, 32, 33,
63, 64 and 4096. Both full lowercase include paths must survive without mutating
the caller's name, and the temporary allocation must be freed for matching and
unmatched modules. The original 32-byte local buffer overflows under ASan.

Descriptor lengths -1, 0, 1, 15, 16 and 17 exercise the fixed Runtab.map[16] ABI.
Invalid non-varargs maps must produce a compiler diagnostic before their table
row is emitted. Varargs rows retain their existing zero-size/zero-map encoding.
The fixture verifies one signature computation and one layout computation per
ordinary row, with signing before layout, preserving polymorphic rewriting order.
Ten accepted generated tables are compiled with strict C99 syntax checks; their
map field is checked to remain exactly 16 bytes.

The default uses fail-fast UBSan. Override CFLAGS to add ASan.
`NATIVE_STUB_SOURCE` selects an old stubs.c for negative probes;
`NATIVE_STUB_MAPS_ONLY=1` skips name generation when isolating descriptor checks.
The corresponding runtime rejects bad table counts and maps; see
libinterp/tests/README.builtin-modules.md. These checks keep the existing native
module ABI and do not execute dynamic object images or new JIT backends.
