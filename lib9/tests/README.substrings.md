# Native substring matching across int-sized length limits

Run `python3 lib9/tests/substrings.py` from the repository root.

The driver compiles production cistrstr from both lib9 and libkern, plus native
strstr/strlen/strchr/strncmp and ASCII bounded comparison dependencies. Symbols
are renamed so the host libc remains an independent oracle. Both signed- and
unsigned-char modes run for each library.

Each configuration checks 402,745 haystack/needle pairs: every haystack of
length 0..4 and needle of length 0..3 over {A,a,B,b,0x80,0xff}. Independent lookup
folding followed by host strstr supplies case-insensitive positions; ordinary
host strstr supplies exact-byte positions. Empty needles, empty haystacks,
repeated prefixes, overlaps, early terminators and high-bit bytes are included.

cistrstr now compares folded byte prefixes without first storing a native
length in int. It stops at NUL, folds ASCII only and retains first-match
semantics. The portable kernel strstr retains its existing search strategy,
but its comparison length is now long, matching native strlen/strncmp.

Exercise genuine large strings on Linux LP64:

```sh
SUBSTRING_WIDE=1 python3 lib9/tests/substrings.py
```

Each library's signed-char configuration then uses a 2,147,483,681-byte needle.
The valid virtual span reuses a 2 MiB file-backed chunk. Its final page is
replaced privately inside the fixture's reservation, so the NUL terminator
appears only at the true end, not in every alias. A production strlen check
confirms the full length. A short haystack cannot contain that needle, while
short matching needles must still match the beginning of the large haystack.
No multi-GiB physical allocation is required. This is a real length test, not
a model that changes integer widths or skips the scan.

Both old cistrstr copies narrow the long suffix length to a negative int and
incorrectly accept a short haystack prefix. Old kernel strstr similarly passes
a narrowed negative count to native strncmp. Each prior body independently
fails the large-needle check.

```sh
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' ASAN_OPTIONS=detect_leaks=0 python3 lib9/tests/substrings.py
```

Small cases pass ASan/UBSan; wide cases pass UBSan. `SUBSTRING_CASE=wide` selects
only large strings in all four configurations. `SUBSTRING_LIB9_CISTRSTR`,
`SUBSTRING_LIBKERN_CISTRSTR` and `SUBSTRING_LIBKERN_STRSTR` select prior bodies;
dependency overrides use the same library/function naming pattern.

ASCII-only semantics and public signatures are unchanged. This does not add
Unicode case folding, a faster worst-case substring algorithm, a native target
ABI test or a new JIT backend.
