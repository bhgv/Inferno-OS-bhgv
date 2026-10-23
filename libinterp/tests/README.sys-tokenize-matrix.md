# Per-input Sys.tokenize allocation-failure matrix

Run `python3 libinterp/tests/sys-tokenize-matrix.py` from the repository root.
This complements [the main tokenizer suite](README.sys-tokenize.md): every input
in this matrix is retried at each allocation boundary, rather than injecting
failures only into one representative multiword result.

Four ASCII/Rune representation combinations, lengths 0..32 and seven patterns
cover embedded NUL, supplementary Unicode and full 32-bit Rune patterns.
An independent scanner checks token characters and order. The old output owns
an input reference; errors must preserve both result fields, input ownership,
allocation counts and payload guards. Successful replacement releases the old
list after publishing its replacement. Nil inputs start with an owned old result;
nil/empty delimiters preserve a whole nonempty source, including embedded NUL.
Each WORD32/native-WORD64 configuration passes 8,322 scenarios.

The driver extracts the current production `buildtokens`, `tokdelim`,
`Sys_tokenize`, `slicer`, string constructors, `cons` and emulator `kprotect`/
`krethrow`. It uses real frame/heap/type declarations with bounded poisoned
allocation and reference/destructor adapters. This is not a collector or native
builtin ABI run. WORD32 retains native host pointers. `TOKENIZE_SOURCE` selects
another runtime source; `TOKENIZE_CASE=functional` omits injected failures.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-tokenize-matrix.py
```

UBSan is the default; this complete matrix also passes ASan+UBSan. LeakSanitizer
process inspection is disabled, but explicit allocation/reference checks remain
active. ASan can report an ignored __asan_handle_no_return request at the host
siglongjmp boundary; stack instrumentation stays enabled. The separate full
heap/slice fault suite has a stricter unresolved ASan limitation documented in
[its guide](README.array-copies.md).

The production tokenizer corrections were already published by commit
`6db2f64c42bc12b9fe9ad60c7633f7a4b0daaefd` while this matrix was being developed.
This extension preserves that implementation and its nil-delimiter semantics.
