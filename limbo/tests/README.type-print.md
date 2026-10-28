# Iterative Limbo Type diagnostics

`python3 limbo/tests/type-print.py` extracts the production `tprint`, its private
continuation stack, `valtmap`, kind names and bounded string helpers. The fixture
links the real lib9 formatting engine. A frozen recursive implementation provides
an independent shallow spelling oracle, including its actual `%T` callback path.

The printer now traverses Type edges with a checked, geometrically grown stack.
Each continuation retains the parent Type and current declaration/argument list.
It preserves tuple, function, generic parameter and instantiation argument order,
without changing compiler-owned Types or declarations. Arrow and dot bases use
the same output span instead of recursively creating a fresh 1024-byte formatter
buffer. Consequently a caller with a larger destination receives the complete
qualified type when it fits, rather than an intermediate truncated base.
Traversal stops when no more output bytes fit and frees all private stack storage.

Per build profile:

- 66,000 differential scenarios: 1,000 reproducible mixed shallow trees, each at
  capacities 0 through 64 and 1024. Trees include instantiated argument lists,
  references, arrays, lists, qualified names, tuples and function arguments,
  returns, generic parameters and varargs.
- 2,146 nominal/scalar/mapped-parameter and invalid-kind scenarios. Mapped generic
  parameters use the real `valtmap` with a nonidentity mapping. The fixture does
  not exercise the separate fixed-point `%v` formatter.
- 14 deep traversals of 100,000 Types with a 512 KiB C stack: instantiated and
  arrow chains, references, arrays, dot qualification, tuples and functions.
  Short and large destination spans check canaries, returned cursors, storage
  ownership and source preservation. Large instantiation, arrow, reference,
  tuple and function outputs also have independent length/content checks.

Total: **68,160 scenarios per profile**, **204,480** for `-O1` and `-O0`
ASan/UBSan plus `-O2` UBSan, all with `-DNDEBUG`. Pointer/integer cast diagnostics
are errors in the optimized profile. The core suite includes this driver.

`TYPE_PRINT_TYPES=/path/to/types.c` selects another production source and
`TYPE_PRINT_CASE=inst` or `arrow` selects a deep regression independently. Both
families reproduce an ASan stack overflow on parent revision `7b5f982` before
this change; neither result relies on a build failure.

This is the Limbo diagnostic printer, not the C stub generator's `ctprint`.
Native bootstrap additionally compiles all 17 Limbo units. This change does not
alter opcode encodings, native fast-path counts or the coordinated 64-bit VM ABI.
