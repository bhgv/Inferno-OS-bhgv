# Compiler-local byte array descriptors

`cvtac` and the Sys UTF helpers accept a module-local descriptor when its element
size is one byte and it contains no GC pointers. Pointer-bearing, nil, H and
wrong-size descriptors remain invalid. This follows typed Limbo array semantics;
it does not admit arbitrary memory or establish bytecode sandboxing.

Run `python3 libinterp/tests/module-byte-descriptors.py`. It derives only fixture
inputs and executes unchanged production C through the existing string-slot and
Sys UTF regression runners, under UBSan in WORD32 and native WORD models. Existing
alignment, alias, ownership and invalid-input cases remain active. No production
source is patched by the runner.

This fixes conversion of byte arrays constructed by compiled Limbo modules,
whose local type identity can differ from the canonical runtime Tbyte address.
