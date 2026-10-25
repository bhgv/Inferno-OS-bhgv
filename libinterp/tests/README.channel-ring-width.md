# Native-width buffered channel indexes and identical-range copies

Buffered sends now wrap the ring index by comparing `front` with `len-size`
before adding. The previous `front+size` could overflow signed `int` even
when both indexes described a valid large ring. Both buffered directions
widen the element index before multiplication by the element size; otherwise
a valid byte displacement above INT_MAX overflowed before pointer arithmetic.
Public Channel and Array layouts are unchanged.

A C-null buffer raises exNilref in cputb and in the native csendalt entry
before operand publication, allocation, queue changes or input destruction.
The language nil sentinel retains its existing unbuffered behavior.

AMD64 SLICELA skips the copy loop when validated source and destination byte
ranges start at the same address. Every geometry, type and span guard still
runs first. The optimization changes no VM storage, reference counts or FP
state and stays inside the existing 64-unit publication limit. ARM64 retains
its tested copy loop: its current emission budget does not accommodate the
extra branch. No publication limit was increased.

The production channel fixture covers large index multiplication, wrap after
an overflowing mathematical front+size, exact front wrap, ordinary indexes,
both transfer directions and eight operand alignments. A small physical
allocation backs the selected slot; fabricated metadata bases use uintptr
arithmetic, so the boundary matrix does not allocate multi-gigabyte arrays.
The parent C implementation reproduces signed overflow under UBSan.
Null-buffer VM sends cover all 64 source/destination alignments and assert
unchanged REG, operand bytes, channel state, queues and scheduler counters.
Native send faults additionally check retained pointer inputs and retry.

```
CHANNEL_TRANSFER_CASE=ring CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-transfers.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-transfers.py
python3 libinterp/tests/slice-copy-jit.py
python3 libinterp/tests/slice-copy-native.py
python3 libinterp/tests/mapped-copy-layouts.py
python3 libinterp/tests/slice-tokenize-suite.py --core
python3 libinterp/tests/jit-coverage.py
```

The emulator's write hook explicitly requires no writes for successful AMD64
identical-range copies, in addition to existing empty/nil/fallback checks.
Whole-image and FP-state comparisons still cover AMD64 and ARM64 with both
WORD models. Native AMD64 comparisons execute actual C and generated code.

No new opcode IDs: AMD64 and ARM64 each have 128 guarded native paths and
45 interpreter-only instructions; RV64+M 103/70, RV64+Zmmul 90/83, RV64I
87/86. These are partial native paths, not a completed native 64-bit VM port.
Physical ARM64/RV64 and complete OS/GC/frame ABI acceptance remain separate.
