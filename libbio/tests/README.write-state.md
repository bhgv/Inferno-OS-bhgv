# Buffered output state and interrupt regressions

Run `python3 libbio/tests/write-state.py` from the repository root.

The fixture compiles production `Bwrite`, `Bputc` and `Bflush` with real Biobuf
declarations. A deterministic native-write boundary injects one interrupted
write without accepting any bytes; an error-text stub identifies the interrupt.

The 148 cases cover capacities 1 through 32 and interrupts on each of the first
three flush attempts. They check the full pending-buffer count, previously
transmitted prefix, file offset, retry flush, subsequent output, final byte order
and guards. Every non-output state is also checked across 17 pending counts:
`Bputc` and nonempty `Bwrite` must return Beof without accepting bytes or attempting
a flush. Ordinary byte-at-a-time output retains Bputc's existing success result.

Defaults use strict UBSan. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libbio/tests/write-state.py
```

`BIO_BWRITE_SOURCE`, `BIO_BPUTC_SOURCE` and `BIO_BFLUSH_SOURCE` select alternate
production bodies. The test does not use a real filesystem or real signal-driven
I/O interruption. Short native writes are injected as described below. This is
not an Inferno boot. LeakSanitizer process inspection is disabled in the sandbox;
ASan/UBSan remain active.

## Short native writes

The driver also bounds each native write to a short positive prefix. Both
`Bwrite` and `Bputc` followed by `Bflush` must emit the complete byte stream
in order. A successful short write advances the file offset immediately;
it is not an output error.

If `Bwrite` encounters an interruption after writing a prefix, it returns
`Beof` and retains only the unwritten suffix in the active output buffer.
An explicit flush and continuation must produce neither duplicates nor
missing bytes. Zero-progress writes return failure instead of spinning.
`Bflush` retains its existing inactive-on-error contract, but accounts for
accepted bytes and keeps its unwritten suffix and count consistent.

The expanded driver covers 2162 cases, including buffer guards and failure
injection after a positive short write. Set `BIO_BWRITE_SOURCE` or
`BIO_BFLUSH_SOURCE` to an earlier production file to reproduce the failures.
