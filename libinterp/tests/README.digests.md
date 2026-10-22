# Keyring/Crypt digest bounds and ownership

Run from the repository root:

```sh
python3 libinterp/tests/digests.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libinterp/tests/digests.py
```

Each of Keyring and Crypt passes 5,762 cases in both native WORD64 and WORD32
scalar models under UBSan and ASan+UBSan. Real generated builtin frames,
Heap/Array declarations and native DigestState layouts are used. WORD32 changes
only WORD/UWORD in a temporary interp.h; pointers remain native. Bounded VM heap,
reference validation and error delivery are fixture models. GC marks, complete
VM stack ABI, scheduling and native JIT fallback execution are outside scope.

The fixture links production SHA1, SHA224/256/384/512, MD4, MD5 and HMAC code,
including their portable compression functions. Independent Python hashlib/hmac
vectors cover abc and lengths 0, 1, 55, 56, 63, 64, 65, 127, 128 and 129, checking
every possible split point. MD4 uses the standard abc and empty-message vectors.
HMAC vectors include empty, ordinary and full 64-byte keys. The historical
64-byte key limit is preserved. All output arrays have checked trailing guards.

Both wrappers retain their old result until validation, allocation and native
work finish. Updating a state whose only owner is the return slot retains the
returned reference before releasing that owner. DigestState.copy copies before
replacing an aliased source. VM allocation failure and invalid types, nils,
negative/full-width lengths, missing data and undersized outputs preserve the
old result and native state. Requests larger than the input retain their existing
clamp-to-input behavior, now before native narrowing. Empty nil input spans are
checked against independent empty-message vectors. All VM/native allocation
counts and type reference counts must balance.

Native portable word loaders promote the high byte to u32int before shifting
into bit 31. Native empty updates use a valid zero-length byte span before any
pointer arithmetic or memmove. HMAC finalization uses a private stack state for
the outer hash, so consuming the inner state is not followed by a fallible native
allocation. With an existing VM state there are no native allocations; one-shot
native HMAC requires only the inner allocation. Tests inject a failure after
that allocation and still require the correct output and zero live native blocks.

Baseline wrapper probes reproduce heap-use-after-free in both modules under
ASan. Native probes reproduce signed high-byte shifts and nil/zero memmove UBSan;
the old HMAC finalization fails the no-outer-allocation test. Cryptographic state
layouts, generated builtin interfaces and ordinary digest outputs are unchanged.
This is host vector/bounds/ownership validation, not complete Inferno boot or a
complete public native hashing API audit. LeakSanitizer process inspection is
disabled for the sandbox; explicit allocation accounting remains active.
