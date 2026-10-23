# Emulator startup parsing

Run from the repository root:

```
python3 emu/port/tests/startup-options.py
python3 emu/port/tests/startup-geometry.py
```

Both scripts accept `CC` and `CFLAGS`; UBSan is enabled by default. Set
`PORTABLE_SOURCE` or `ANDROID_SOURCE` to test an alternate launcher revision.
ASan can also be enabled. LeakSanitizer is disabled in these fixtures because
process inspection is unavailable; neither fixture allocates heap blocks.

The option suite extracts the argument-vector statements from each launcher
and the actual `qtoken`/`tokenize` functions from lib9. A bounded adapter checks
the tokenizer's capacity contract, and a dispatch stub verifies argv[0], all
words and the terminating nil. The usage-exit boundary is mapped to a rejected
result. These are extracted parsing blocks, not a complete emulator launch.

The 1,542 cases cover zero through 256 words, whitespace, quoted spaces and
doubled quotes. A 20-slot vector reserves its first slot for `emu`. Tokenizing
up to 19 words allows the launcher to detect excess input; 19 or more words
are rejected before writing the terminator or dispatching options. Up to 18
words leave room for that terminator. The old byte-count argument fails the
capacity assertion in both launchers, without letting it overwrite the fixture.

The geometry suite extracts both production `isnum`/`geom` implementations.
Its 1,610 cases use labelled spelling/range expectations, check both dimensions,
and ensure rejected strings leave the current geometry unchanged. Cases include
minimum-size thresholds, accepted octal/hexadecimal spellings, null/empty input,
trailing junk, signed values and numbers exceeding signed-int or native-ulong
ranges. Existing base-zero width parsing and decimal-digit-only height syntax
are preserved, including their historical hexadecimal-prefix interpretation.
The old source fails the transactional-state assertion; wide values such as
`4294967936x480` must also be rejected before narrowing to an int.

Host syntax checks cover the portable launcher. Android parsing is executed by
the fixture without requiring Android headers or claiming an Android build.
