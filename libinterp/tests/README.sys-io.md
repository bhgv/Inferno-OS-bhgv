# Sys native I/O count regression tests

Run `python3 libinterp/tests/sys-io.py` from the repository root.
The driver separately compiles the production `Sys_read`, `Sys_readn`,
`Sys_write`, `Sys_pread`, `Sys_pwrite`, and `sysiocount` from both
`emu/port/inferno.c` and `os/port/inferno.c`, with real generated frames and
heap/array declarations.

For each implementation, WORD64 and WORD32 scalar models check 90 cases:
ordinary counts, counts above 32 bits, negative counts that previously wrapped
to positive values, array clamping, nil-buffer conventions, malformed array
metadata, empty spans, short reads, initial errors, and errors after progress.
The native-call fixture records byte spans and 64-bit file offsets, and checks
that validation happens before releasing the interpreter. It also verifies
that `readn` accumulates short reads and returns progress after a later error.

Defaults enable strict UBSan. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-io.py
```

The native I/O and scheduler boundaries are bounded stubs. This does not boot
an emulator or kernel, perform real filesystem I/O, or execute GC/JIT builtin
calls. WORD32 retains host pointers and is not a complete 32-bit ABI test.
LeakSanitizer process inspection is disabled for the sandbox; ASan and UBSan
remain enabled. `SYS_IO_SOURCE` selects an alternate source for regression
comparisons.
