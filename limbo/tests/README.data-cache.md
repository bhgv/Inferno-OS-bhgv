# Dis data-cache growth and interface serialization

Run from the repository root:

```sh
python3 limbo/tests/data-cache.py
```

The driver runs 219 cache/header scenarios and ten interface-global scenarios.
It compiles production data-header/operand encoding, byte and scalar writers,
string emission and cache flushing. Canonical real conversion uses production
dtocanon. An independent decoder checks counts, offsets, record boundaries,
payload bytes and canonical numeric byte order.

The cache now grows to the complete requested span instead of repeatedly
requesting ncached+1024. In particular, a large disbytes request or an exact
1024-byte request can no longer stall at an unchanged capacity. Geometric growth
is bounded by the signed positive Dis count range. Counts and offsets are
validated before cache arithmetic; record header fields are checked before
emitting their first byte. The capacity is published only after allocation
succeeds. Empty byte spans access no source and do not split a pending record.

Coverage includes exact capacity transitions, initial and existing cache
contents, blocks through 65,536 bytes, noncontiguous/rewound offsets, byte/word/
big/real records, negative zero, UTF strings, empty strings and wire-header
count transitions. Header-only cases check field packing, not support for
future data-record kinds in the loader. Invalid spans/counts/kinds must fail
before output or allocation. Injected allocation failures check requested
capacities including 536,870,911 without allocating that large payload.

The interface fixture uses actual limbo.h Node, Decl, Type, Sym and Biobuf
layouts, then calls production disvar -> disdatum -> data writers. It emits
module and qualified ADT interface names of lengths 1023, 1024, 1025, 8193 and
65536, checking complete output bytes including signatures, prefixes and
terminators. Signature generation, alignment and native output are adapters;
parsing and type checking are not executed by this fixture.

The cache fixture uses a moving allocator with guard bytes. A 32-request budget
turns the prior infinite allocation loop into a deterministic regression failure.
The interface fixture independently reproduces the old loop. Old empty-span
handling triggers UBSan on a null source, old malformed spans/headers fail the
rejection checks, and an injected old allocation failure leaves a published
capacity without corresponding storage. Compiler fatal exits are intercepted
only for testing; this does not promise compilation can resume after fatal.

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 limbo/tests/data-cache.py
```

`DIS_DATA_SOURCE` selects an alternate dis.c. `DIS_DATA_CASE` selects `blocks`,
`mixed`, `empty`, `invalid`, `allocation`, `headers`, `fragmented`, or `interface`.
Default execution runs both fixtures. Complete changed translation units also
pass host-header syntax checks. This is not a full Limbo command-line compiler
build, Inferno boot, generated JIT ABI check or a new native backend. The Dis
wire format and runtime layouts are unchanged.
