# GUI device regressions

Run from the Inferno repository root:

```sh
python3 emu/port/tests/guiread.py
python3 emu/port/tests/guiwrite.py
CFLAGS='-O1 -g -fsanitize=undefined' python3 emu/port/tests/guiread.py
CFLAGS='-O1 -g -fsanitize=address,undefined --param=asan-stack=0' \
    python3 emu/port/tests/guiread.py
```

Both entry points run the same host suite. Python 3 and a C compiler are needed.
Build files use temporary directories. No GUI or emulator is started.

The suite extracts production queue, read, write, close, directory generation,
create and remove code. It uses real project headers and production lib9
formatters; process lookup, scheduler and Tk execution are stubs. It covers
partial/zero reads, FIFO/full-Qid isolation, coherent chunked snapshots, long
getters, escaped snapshot round trips, immutable bounded write input, malformed
requests, the 99-command boundary, positive stream offsets, large PIDs,
immutable directory lookup, stable Qids across sibling deletion and object
reuse, independent-open reply ownership, unknown types, long paths, missing widgets/processes,
Tk errors, allocation failures, explicit heap accounting and lock/error balance.

Successful Draw/Tk bootstrap, real resource reference counts, rendering, GC and
full emulator integration remain untested. ASan stack instrumentation is disabled
because sandbox `/proc` limitations interfere with its siglongjmp stack handling;
explicit allocation accounting substitutes for unavailable LeakSanitizer.
See [the audit](../../../doc/devgui-audit.md) for closed defects and remaining work.
