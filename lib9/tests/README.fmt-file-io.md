# Formatting-buffer file I/O regressions

Run from the repository root:

```
python3 lib9/tests/fmt-file-io.py
```

The fixture compiles production _fmtFdFlush, fmtfdflush and fmtfdinit bodies
with the real Fmt type. It runs native and ANDROID branches with controlled
write/logging boundaries. ASan and UBSan are enabled by default; CC and CFLAGS
can override the compiler.

Byte-exact oracles cover short writes, zero progress, errors after a written
prefix and retry. A failed flush must retain exactly the unwritten suffix and
must not duplicate already written bytes. Android logging is tested with full,
non-NUL-terminated buffers; only stdout/stderr use logging. Other descriptors
must reach native write. Empty flushes must not inspect uninitialized data.

FMT_FILE_IO_SOURCE and FMT_FILE_INIT_SOURCE can select earlier source versions.
No Android logging library or live descriptor is used; these are host tests of
production buffer handling, not an Android build.
