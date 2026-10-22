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
I/O interruption, and does not test partial successful native writes. This is not
an Inferno boot. LeakSanitizer process inspection is disabled in the sandbox;
ASan/UBSan remain active.
