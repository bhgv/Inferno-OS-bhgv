# String and compiler integer regressions

Run from the repository root:

```sh
python3 libinterp/tests/strings.py
python3 libinterp/tests/string-codecs.py
python3 limbo/tests/fold-integers.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libinterp/tests/strings.py
```

Both drivers compile the actual production function bodies. The string fixture
uses the real VM headers with a bounded allocator, exact capacities, payload
guards and reference accounting. It covers self append with and without spare
capacity in ASCII/rune storage, nil insertion, shared-destination bounds failures,
allocation-size rejection, negative rune-count compatibility, UTF buffer-size
rejection and signed big conversion extrema.

The compiler fixture supplies a minimal AST and diagnostics around `foldc`,
checking 398 shift, sign-extension, width and wraparound cases under UBSan by
default. These tests do not build the complete Limbo compiler or boot Inferno.

The tests fail against the pre-fix production sources: self append changes bytes
beyond the result, and big right-shift folding shifts an `int` mask by 63 bits.
The string harness disables LeakSanitizer's process inspection because this
sandbox does not expose `/proc`; its own allocation/reference checks must balance.

These changes preserve the current Dis layouts and instruction set. They do not
introduce a new pointer-width ABI or implement an x86-64/ARM64 JIT. Keeping the
integer folding rules independent of host `int` is necessary for those future
backends and the existing RV64 backend.

The separate `string-codecs.py` fixture compiles the real string constructors,
character store, word formatter, byte/string conversion instructions, cached
UTF view, mem2array and lib9/rune.c. It uses real Heap/String/Array/REG layouts
with bounded heap allocation, exact capacities, guards, reference accounting
and deterministic VM heap failures. WORD64 and WORD32 scalar models each pass
39,771 checks under UBSan and ASan+UBSan. WORD32 changes only WORD/UWORD in a
temporary real header while pointers remain native; this is not Dis ABI32
execution.

Coverage includes Unicode boundaries, surrogate halves, values above Runemax,
full 32-bit rune patterns and 4096 deterministic generated cases split between
Unicode-range values and unrestricted values. An independent UTF encoder
checks exact bytes. High-bit runes remain in rune storage until the encoder
replaces invalid values, rather than becoming negative C ints and falling into
an ASCII path. Byte exports and cached C strings must agree, including embedded
NULs; existing incomplete trailing UTF-sequence behavior is preserved.

Byte imports validate element/heap types and full VM-word lengths before native
int conversion. Native c2string rejects negative lengths, impossible payload
sizes and nonempty null buffers before scanning. Nil VM arrays still yield nil
strings; empty non-nil arrays yield allocated empty strings. Empty native null
spans avoid null pointer arithmetic. Rune exports and cached views reject
impossible negative lengths before negation or iteration. Invalid descriptors
use poisoned data pointers to prove rejection happens before reads.

Word formatting now uses an explicitly widened signed argument and enough room
for every supported WORD value. Allocation-failure checks preserve old output
and exercise successful and failing conversions when source and destination
are the same register slot. All fixture VM references balance. Garbage-collector scheduling and complete Inferno boot remain outside
this fixture. ASan stack instrumentation is enabled for this setjmp-based test;
LeakSanitizer inspection remains disabled for the documented sandbox limitation.
The baseline bodies of all six changed codec/store functions independently fail
the new fixture.

Character stores now prepare their final capacity and representation before
publishing a copied result. The fixture sweeps lengths 0 through 20, all valid
update/append positions, shared/unshared ownership, ASCII/Rune sources, ASCII
and non-ASCII characters, and first/second allocation failures. Each store
needs at most one allocation; rejected stores retain the old result and shared
source bytes. Allocated empty ASCII strings with spare byte capacity are
converted before Rune stores, so byte capacity is never reused as Rune capacity.
A separate append sequence checks this capacity invariant and every heap guard.

Character reads and stores validate complete signed VM indices before accessing
storage. Added index cases include negative values and high bits above 32 bits;
errors must preserve output values, ownership, bytes and allocation counts.
The old store body independently fails the extended fixture. These checks still
use bounded heap/GC stubs rather than actual JIT execution or a complete boot.

Native UTF cache allocations now have independent deterministic failure injection.
For 64 strings in each word model, the driver rejects first allocation, builds a
cache, rejects replacement, checks retained pointer and bytes, retries, and frees
the string. `msize` deliberately reports zero to force the replacement path.
The previous implementation fails the retained-pointer assertion. This extends
the driver to 39,963 checks per model; ASan/UBSan and VM reference accounting pass.

Packed borrowed headers and Rune payloads are additionally checked by
[string-view-native.py](README.string-view-native.md). It covers byte-safe UTF
cache publication and comparison, failed cache replacement, and aligned Heap
ownership for duplicates with active release-mode predicates in both models.
