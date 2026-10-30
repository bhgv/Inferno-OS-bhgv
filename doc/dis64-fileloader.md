# Experimental Dis64 file routing

`DIS64_PILOT_LOADER=1` enables the agreed Dis64 discriminator in the existing
production parser. It is a private build selection, not a wire flag, extension,
new ABI number or default format change. Values 0/2 are rejected at compilation.
A compatible runtime also needs WORD/UWORD32, pointer64 and the other native
model checks in prepare64pilot; use DIS_WORD32=1 consistently across its objects.
Changing these selections requires a clean, coherent build of all consumers.

| Build | Valid scalar Dis64 file |
|---|---|
| File-routing macro absent | Existing behavior; no Dis64 Module is published |
| Routing enabled, legacy LP64 WORD64 | Rejected by native-model admission |
| Routing enabled, WORD32/ref64 | Existing admission predicate, then semantic/runtime checks |

The optional branch recognizes the exact eight-byte magic before the legacy
operand decoder. A recognized new-format image never falls back to that decoder
when its version, ABI, profile, roles or native model is unsupported.
The shared wire codecs, registry, 43-op scalar subset and native restrictions
remain unchanged.

## Existing reader, admission and cache

readmod still opens/stats/reads/closes the file through its existing path.
The existing unsigned-file admission predicate is applied before constructing
Dis64 owners, just as for unsigned legacy Dis. Its implementation and all
signature/cryptographic code are unchanged. This series assigns no signing
section or alternate admission policy.

prepare64pilot still returns an unpublished owned Module. Only successful
file parsing copies dev/type/Qid/mtime and inserts that owner into the ordinary
module cache. The caller owns the returned reference. Failed admission or
construction leaves no new cache entry. The existing reader's input-reference
consumption and identity-hit/refresh rules remain in force.

The image buffer and Dir are released by readmod after parsing. Runtime
instructions, scalar data, descriptors, name and path therefore must remain
usable without retaining those borrowed buffers. Instance MP remains a separate
clone made by linkmod, with normal scheduler ownership and cleanup.

A compilation request uses existing cflag/dontcompile controls. Requested native
compilation failure fails construction; dontcompile deliberately suppresses the
request. Leaf JIT preserves compiled=0 and original Dis PCs. No multi-opcode block
compiler is introduced. Existing file-size resource limits remain in force and
are not the container's normative UINT32_MAX limit.

## Reproducible connected checks

```
python3 libinterp/tests/dis64-fileloader.py
CFLAGS='-O3 -g -DNDEBUG -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis64-fileloader.py
python3 libinterp/tests/word32-build.py
```

The connected driver uses actual readmod, both parser branches, prepare64pilot,
lookmod/unload and the existing scheduler fixture. Its host read/stat/close
boundaries are controlled; the production read loop receives short replies.
The two original goldens, zero-divide and invalid-shift images then execute
through actual C/AMD64 RX startup/dispatch/terminal cleanup.

Checks cover Qid paths above 4 GiB, metadata identity hits and refresh, cache
reference counts, all truncated prefixes, no legacy fallback on unsupported
versions, admission callback refusal/error, every tracked file/construction
allocation failure, native request/suppression, short reads 1/3/7 bytes and
source immutability. A retained cache owner survives each failed new load.
The serialized image is freed before process execution.

Default and O3/NDEBUG UBSan pass 187,861 active predicates across four enabled
images, plus separate valid-image gate checks for macro-off and legacy WORD64.
The enabled chain includes 48 terminal process deletions, 40 final exit requests,
24 actual native calls (12 accepted/12 declined), 60 C calls and 12 complete tails.
The object probe builds 20 full units in each of three selections (60 objects).

The legacy parser matrix also passes in WORD32/legacy × routing off/on:
120 truncations and 30,720 byte mutations per selection, plus canonical big-bit
boundaries and allocation/data-map checks. Canonical DEFL values now assemble
in unsigned64 storage and copy exact bits, avoiding intermediate signed WORD
narrowing and out-of-range unsigned-to-LONG conversion.

This is Linux/AMD64 connected-component evidence. Native shared libraries,
real filesystem/worker blocking, live GC/compaction, generated builtin-interface
compatibility and a full emulator boot remain separate checks. No inspector execution_supported flag,
reference/calling ABI, default target, ARM64/RV64 physical acceptance or legacy
Dis pointer-layout compatibility is inferred from the scalar tests.

The newer [vmachine gate](dis64-vmachine.md) connects this same file route to
the unchanged vmachine handler/propex loop, including retained broken roots,
group propagation and separately injected legacy in-memory exception tables.
It does not assign or enable a Dis64 control-section format.
