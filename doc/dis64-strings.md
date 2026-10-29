# Dis64 string pool payload

Wire agreed in [the payload exchange](dis64-flashvm-payload-response.md),
FLASHVM-0006 in [the journal](dis64-flashvm-journal.md). It extends the accepted
[Rune record contract](dis64-contract.md) without adding IDs or opcodes.

The strings section is BE u32 pool_count, followed by exactly that many non-nil
records. Each record is BE u32 rune_count and exactly rune_count BE u32 values.
All bytes must be consumed. Pool IDs are ordinal 0..pool_count-1. Duplicate
contents under distinct IDs are allowed; serialization does not intern or
normalize them. Count0 yields the four bytes 00000000. A pool containing one
empty string is 00000001 00000000. Neither representation assigns a nil tag.

`include/dis-string-pool.h` shares the existing Rune codec and signed32 order:

- disstringpoolcheck validates complete bytes with an explicit record limit.
- disstringpoolget resolves an ID into a borrowed DisRuneView.
- disstringpoolcompare resolves two IDs and compares their contents.
- disstringpoolwrite concatenates complete validated input records canonically.

Views borrow immutable input for their lifetime; the helper allocates nothing.
Failed check/get/compare does not publish an output. Writer validates every
record and all lengths/capacity before changing output or published length.
Input records, descriptor array, output and published-length storage must not
overlap. The caller supplies accessible spans and keeps them stable during calls.
No C pointer, host endian, padding or native nil representation enters the file.

ID lookup scans records and is linear in the requested index. A runtime can
build a separate validated offset index for repeated access; it must preserve
wire ID order and input lifetime. This helper is a codec, not a VM heap/index
constructor or an interpreter implementation.

Run from the repository root:

```sh
python3 libinterp/tests/dis-string-pool.py
python3 libinterp/tests/dis-rune-case.py
CFLAGS='-O3 -g -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all' python3 libinterp/tests/dis-string-pool.py
```

The first test uses independent BE/Python-integer oracles and verifies failure
atomicity, limits, all record IDs, duplicate/empty records and unaligned input.
The second composes the real pool writer/reader/comparator with the typed table
validator and Dis-PC selector. It does not execute new-ABI module code.
