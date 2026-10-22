# Unsigned 9P wire integer decoding

Run `python3 lib9/tests/wire-integers.py` from the repository root.

The production GBIT32/GBIT64 macros from fcall.h and styx.h each run 67,584 unaligned little-endian vectors:
every byte position with every byte value and 65,536 deterministic mixed-byte
patterns. An independent multiplication-based unsigned decoder provides the
expected values. Each pattern also runs a real Dir encode/statcheck/decode
round trip, including high bits in device, qid, timestamps and mode fields,
64-bit qid paths, positive/negative lengths and all four strings. Both lib9 and
libkern conversion bodies run against each production header independently. The old styx.h decoder also
sign-extended the low 32-bit half into the high half of a 64-bit value.

Strict UBSan is the default. ASan can be enabled with
`CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'`.
LeakSanitizer process inspection is disabled; other instrumentation remains on.
`WIRE_FCALL_SOURCE` and `WIRE_STYX_SOURCE` supply alternate headers. The original GBIT32 and GBIT64
macros independently fail UBSan on high-byte shifts. The fix retains the wire
format and reads the same eight/four bytes. These tests do not model network I/O,
malformed message parsing, a running filesystem or a native kernel boot.
