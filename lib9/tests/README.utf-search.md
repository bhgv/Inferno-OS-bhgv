# Native UTF prefix and search regression tests

Run `python3 lib9/tests/utf-search.py` from the repository root.

The fixture links production `utfnlen`, `utfrune`, `utfrrune` and `rune.c`.
Its 88 cases check exact UTF byte boundaries, incomplete sequences, embedded
malformed bytes, ASCII/Unicode first and last positions, terminator searches,
negative/out-of-range rune values, and zero/negative/very large length limits.
The existing decoder lookahead convention for malformed leading bytes and
truncated NUL-terminated strings is preserved.

`utfnlen` uses a remaining-byte count, so short NUL-terminated strings accept
LONG_MAX limits without first forming an out-of-object end pointer or narrowing
the limit into `fullrune`'s int argument. Empty/nonpositive spans need no data
pointer. Search functions reject invalid rune values before libc can narrow
a negative value into a matching byte.

Defaults enable strict UBSan. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 lib9/tests/utf-search.py
```

`UTF_UTFNLEN_SOURCE`, `UTF_UTFRUNE_SOURCE` and `UTF_UTFRRUNE_SOURCE` select
alternate source files for independent before/after comparisons. The old bodies
independently fail these tests. This uses the host C ABI, not a complete Inferno
build or a VM/JIT builtin path. LeakSanitizer process inspection is disabled in
the sandbox; ASan/UBSan remain enabled.
