# Subfont ownership and path regressions

Run from the repository root:

```
python3 libdraw/tests/subfont-lifecycle.py
python3 libdraw/tests/subfont-paths.py
CFLAGS='-O1 -g -funsigned-char -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libdraw/tests/subfont-lifecycle.py
```

Both fixtures compile production source bodies with the public draw structures.
ASan and UBSan are enabled by default; CC and CFLAGS can override the compiler.
Display, file, and filesystem access are boundary stubs, not live services.

The lifecycle fixture covers newly loaded versus caller-owned images, display
locking, allocation failures, truncated reads, malformed fixed-width headers,
representable field endpoints, cache replacement, final-reference cleanup,
short writes, and failed TrueType cache-key allocation. Allocation counters
check exact ownership independently of LeakSanitizer. Character ascent bounds
follow the actual signedness of the existing public char field; no ABI changes
are introduced.

The path fixture compares every attempted path with an independent expected
path, including absolute names, relative names, root directories, UTF-8 parent
directories, names longer than 128 bytes, depth restrictions, missing files,
and allocation failure. Special *default* lookup bypasses filesystem access.

SUBFONTNAME_SOURCE, READSUBFONT_SOURCE, SUBFONT_SOURCE, FREESUBFONT_SOURCE,
SUBFONTCACHE_SOURCE, WRITESUBFONT_SOURCE and FONT_SOURCE can select earlier
implementations to reproduce the failures independently. These tests do not
rasterize glyphs or contact a running display server.
