# Compiler root and source-path handling

Run `python3 limbo/tests/source-paths.py` from the repository root.
The fixture runs production `getroot`, `win2inf`, `cleann`, and `srcpath`
from both the grammar and checked-in generated parser. It also includes the
real lib9 `cleanname` implementation. Both driver versions must match.
Each signed/unsigned-char build executes 340 cases.

The cases cover repeated EMU root parsing without modifying environment
storage, attached and separated `-r` arguments, spaces/tabs, ROOT fallback,
and allocation failure. Path tests cover complete root components versus
similarly named sibling directories, root equality, normalized dot segments,
drive-letter paths, default roots, and output capacities from one to forty
bytes. Nonpositive output lengths must neither write nor allocate.

A working directory longer than the previous 256-byte buffer must resolve;
a genuine cwd lookup failure must produce a compiler fatal diagnostic after
releasing the temporary path. Tracked allocation ownership must balance.

`SOURCE_PATHS_SOURCE` selects an earlier driver. `SOURCE_PATH_CASES=roots`,
`paths`, or `cwd` isolates a failure family. `CC` and `CFLAGS` override the
compiler and default UBSan flags; ASan is supported with unsupported
LeakSanitizer process inspection disabled. Environment and cwd callbacks are
injected boundaries; this is not a complete compiler or filesystem test.
