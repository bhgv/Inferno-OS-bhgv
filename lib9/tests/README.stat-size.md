# Representable 9P stat sizes

Run `python3 lib9/tests/stat-size.py` from the repository root.

Both lib9 and libkern production sizeD2M/convD2M bodies run 2,496 output-span
cases each. The fixture independently applies the 65,535-byte stat count limit
(the two-byte count itself is excluded). It tests each individual string
position, sums near the limit, 256 deterministic partitions among four strings,
nil combinations and individually overlong strings.

Representable entries retain their size report in the first two bytes on a
short output buffer. Exact and larger output spans must produce a valid entry
accepted by real statcheck/convM2D, including correct recovered strings. An
unrepresentable entry returns zero from both sizing and packing, and packing
must leave the entire output untouched. Each original packing/sizing body
independently fails this regression.

Strict UBSan is the default. ASan can be enabled with
`CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'`.
LeakSanitizer process inspection is disabled; other instrumentation remains on.
`STAT_D2M_SOURCE` and `STAT_M2D_SOURCE` select individual alternate source files.
The 65,537-byte maximum tested here is the direct stat-entry limit; an enclosing
Rstat/Twstat message has its own 16-bit nstat limit. This fixture does not test
network transport, malformed stat decoding, running filesystems or kernel boot.


The decoder regressions also cover every truncated prefix, inconsistent
record-length declarations, overflowing counts in each of the four string
positions, and consecutive records. `convM2D` must respect the first record's
own declaration and validate it before altering either the Dir or string
storage. Nil input/result pointers are rejected; nil string storage retains
the existing empty-name behavior. `statcheck` advances only after each count
fits the remaining bytes. Both library variants now run 2642 cases each.
