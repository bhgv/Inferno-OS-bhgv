# Buffered seek failure and arithmetic regressions

Run `python3 libbio/tests/seek.py` from the repository root.

The fixture compiles production `Bseek`, `Boffset` and `Bflush` with real Biobuf
declarations and deterministic native seek/write boundaries. Its 85 cases check:

- Every position within a 16-byte read window, in active and EOF states, using
  absolute and relative seeks, with exact counts and logical positions.
- Failed native seeks to nearby, distant 64-bit and negative offsets, retaining
  the complete read-buffer metadata and state.
- Signed overflow rejection before native relative seeks and valid arithmetic
  for the minimum signed offset.
- Successful movement outside the window and EOF-relative native movement.
- Output flush failures preventing seeks, native seek errors retaining the
  post-flush offset, and successful output-relative movement after a flush.

The old Bseek independently fails the fixture. `BIO_BSEEK_SOURCE` selects an
alternate source for regression comparisons. Strict UBSan is enabled by default.
For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libbio/tests/seek.py
```

These are deterministic native-call models, not actual filesystem/pipe tests or
an Inferno boot. LeakSanitizer process inspection is disabled in the sandbox;
ASan/UBSan remain enabled.
