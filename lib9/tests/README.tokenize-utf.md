# UTF-8 tokenization and native Rune lengths

Run `python3 lib9/tests/tokenize-utf.py`. It compiles both real native-library
copies of `tokenize.c`, `rune.c` and `utfrune.c`, under signed and unsigned plain
char. A separate Python scalar decoder and quote parser generates 70,380 vectors
per configuration. Coverage includes exhaustive short strings, two/three/four
byte delimiters, lookalike UTF-8 bytes, empty delimiter sets, malformed/truncated
sequences, doubled and unmatched quotes, empty quoted strings and argument limits.
The C runner checks token values, preserved offsets for gettokens, array guards
and buffer guards. `TOKENIZE_SOURCE` selects an old source for negative probes.

gettokens retains quotes and historically leaves the final remainder uncut when
its argument capacity is reached. tokenize removes quoting and uses only ASCII
whitespace. These behaviors are part of the oracle. Unicode delimiters are
matched as complete decoded scalars, including the native Runeerror convention
for malformed input; individual UTF-8 bytes are not delimiter characters.

Run `python3 lib9/tests/runestrlen-width.py` for 512 short lengths in each library.
Set `RUNESTRLEN_WIDE=1` to additionally test 2,147,483,681 Rune values on LP64.
An owned virtual reservation reuses a shared 2 MiB backing chunk, with a private
final page for the unique terminator, avoiding an 8 GiB physical allocation.
Only pages inside that reservation are replaced with MAP_FIXED. The original
int counter fails UBSan at INT_MAX. `RUNESTRLEN_SOURCE` selects a previous source.

Default builds use fail-fast UBSan. Override CFLAGS to add ASan; run the short
Rune cases under ASan and the optional large virtual span under UBSan. These
fixtures test the production native routines, not an Inferno boot or a JIT backend.
