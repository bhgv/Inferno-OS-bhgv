# Include paths and line directives

Run `python3 limbo/tests/lexer-paths.py` from the repository root.
The fixture extracts production include resolution, comment parsing and source
line lookup. Its 15 scenarios cover empty and embedded-NUL include names,
4095/4096-byte paths, prefix overflow, absolute names, and fallback past an
overlong search directory. The filesystem stub records every open attempt:
truncated names must never reach it.

Line tests cover zero, ordinary and INT_MAX logical lines, overflowing decimal
numbers, long directives and long ordinary comments. The parser must consume
the complete comment before rejecting a directive. Diagnostic arithmetic
clamps at signed-int bounds after computing in the compiler's 64-bit Long;
it must not overflow when the line after an INT_MAX directive is queried.
Original physical coordinates are computed in the wide type before clamping.

`LEX_PATHS_SOURCE` selects older production source. `LEX_PATH_CASES=includes`
or `LEX_PATH_CASES=directives` isolates a failure family. `CC` and `CFLAGS`
override the compiler and default UBSan flags; ASan is supported. Filesystem
I/O and mapping allocation are fixture boundaries, not a full compiler build.
