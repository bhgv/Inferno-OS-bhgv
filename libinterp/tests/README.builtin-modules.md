# Native module construction and dynamic image ownership

Run `python3 libinterp/tests/builtin-modules.py`. It extracts actual constructors,
export installation, descriptor allocation/freeing, module lookup/unload and the
VM error-frame helper. The Runtab layout comes from the compiler-generated Sys
header. Each test runs against the real Plan9 and Inferno dynamic-loader bodies.
The fixture adapts allocation, object loading/import lookup, error text and debug
formatting; it does not load or execute target native machine code.

Allocation cutoffs cover module allocation, both owned strings, the descriptor
array, the export array, every dtype allocation, every export name and the final
dynamic path replacement. An allocator observer rejects any partially built
module that appears in the public module list. Every failed attempt must retain
the existing list, leave zero owned allocations, balance the VM error stack and
release an acquired dynamic image exactly once. Successful unload must also
release all native frame descriptors, which are now owned by Module.type.

Both explicit and sentinel-counted tables are tested, including empty tables,
count mismatches, negative/INT_MAX counts, missing callbacks, negative sizes,
out-of-range bitmap lengths and invalid bitmap bits, plus valid 15/16-byte maps. Multiple loaded modules
survive failed construction, and unloading a middle node preserves the chain.
Names/paths outlive mutable caller storage. Repeated export-table cleanup and
exact 63-byte import-name boundaries are checked. The fixture reports 72 cleanup
checks per dynamic-loader implementation, in addition to its field assertions.

newmod now returns unpublished storage with owned name/path strings. Its sole
in-tree caller, builtinmod, publishes only after every export and frame exists.
This contract is also documented at the public declaration. Builtin frame types
are retained in Module.type/ntype, so descriptor introspection can now enumerate
them instead of reporting an empty table; link frames borrow those module-owned
descriptors just as ordinary Dis exports do. No structure or bytecode ABI changes. The matching compiler-side checks are
described in limbo/tests/README.native-stubs.md.

The default uses fail-fast UBSan; override CFLAGS for ASan as well. The allocator
preserves Inferno malloc's zeroing contract and rejects frees of borrowed storage.
`BUILTIN_LOAD`, `BUILTIN_RUNT` and `BUILTIN_DYNAMIC` select older production files
for negative probes. The old constructor exposes a partial module during its
next allocation; the old dynamic loader dereferences nil after a failed builtin
construction. `BUILTIN_BAD_COUNT=1` isolates a table whose declared capacity is
smaller than its sentinel-counted length; the old constructor writes beyond its
export allocation under ASan. Tests do not boot Inferno, execute native exports, validate actual
object signatures/relocations or implement any new JIT backend.
