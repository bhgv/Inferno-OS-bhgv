# Dis input and initialization audit

## Closed defects

`parsemod` previously decoded operands and fixed-width values before checking
input length. Opcode/address bytes, type maps, data payloads, names, import and
exception trailers could also run past the supplied buffer. Numeric decoders
now have width checks at every call site; raw bytes and strings are bounded by
the same end pointer. Negative signature lengths and overflowing table/count
arithmetic are rejected before allocation.

Export Type/PC indices and handler Type/PC/range/count metadata are validated
before indexing. Exception range ends may equal nprog; the default handler
retains its unsigned -1 sentinel. Unknown opcode numbers are rejected. This is
not a verifier for all instruction operands or run-time program behavior.

Data initializer addresses now track the remaining region size, including
DIND/DAPOP through nested arrays. Scalar writes must be aligned, fit the region
and avoid GC pointer slots. String/array destinations must be marked pointer
slots. Negative/overflowing array lengths and out-of-region indices are
rejected. Replacing a string/array releases the previous value. The active
bitmap still uses native pointer-width slots; no Dis64 ABI is enabled here.

The whole parse runs through the runtime error-protection boundary. The owned
Module remains outside the setjmp frame so heap/string/link/compiler errors
clean up data, exports, descriptors and partially populated metadata. Pointer
and sentinel tables are explicitly zero-initialized, allowing cleanup after
any allocation cutoff. Failed parsing no longer leaks export names/tables.

## Verification and limits

The production parser, scalar decoders, descriptor constructor, initializer
helpers and link routines run with real VM structures and emulator Proc error
frames. The fixture contains pointer/scalar data, string replacement, arrays,
DIND/DAPOP, an export, imports and exception metadata.

- Every one of its 120 proper prefixes is rejected with no live allocations.
- 30,720 deterministic single-byte mutations are parsed, with allocation,
  error-frame and static reference-count checks after each result.
- Allocation cutoffs exercise ordinary failures and VM error unwinds in heap,
  string, array, link and metadata construction. Separate compiler errors are
  injected. A host allocation ceiling of one MiB bounds mutated fixtures.
- The successful fixture checks nested scalar values, a scalar module field,
  an end-PC handler range, and complete destruction after publication.

```sh
CFLAGS='-O2 -g -fsanitize=undefined' python3 libinterp/tests/dis-loader.py
CFLAGS='-O1 -g -fsanitize=address,undefined --param=asan-stack=0' \
  python3 libinterp/tests/dis-loader.py
```

ASan stack instrumentation is disabled for longjmp because the sandbox lacks
its expected /proc stack information. Live-allocation accounting substitutes
for unavailable LeakSanitizer. These checks use injected heap, signing and
compiler boundaries; they do not execute arbitrary mutated bytecode, perform
full GC, verify real signatures or boot a complete emulator/kernel. Real
32-bit builds and compatibility over a broad corpus of compiler-produced Dis
modules remain integration work. LP64 host fixtures follow the active legacy
layout, not ordinary ABI32 module compatibility or the proposed Dis64 ABI.
