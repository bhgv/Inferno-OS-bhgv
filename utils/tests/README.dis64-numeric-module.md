# Private numeric storage / Module / C and RX execution

Run from the repository root:

```sh
python3 utils/tests/dis64-numeric-storage.py
python3 utils/tests/dis64-numeric-module.py
python3 utils/tests/dis64-numeric-exec.py
```

The storage matrix independently serializes raw images and checks numeric intervals
against mapped reference slots, frame locals, DATA and interfaces. Controls are
`--control map|frame|fp|data|interfaces`. The Module fixture runs actual storage,
Heap/Type/collector/unload bodies with native allocation fault injection. Controls
are `--control entry|path|partial|lease`. It uses jit=0; its compile stub refuses.

The separate execution fixture runs the actual VM, error/unwind and native JIT
bodies. It requires completed fast returns from actual RX mappings. `--control`
removes entries and must fail the exact RX-completion predicate despite correct C
fallback. Collector execution is deliberately separate: no GC-during-execution,
managed frame roots or scheduler acceptance follows. Divide-zero is an actual
native decline followed by the C runtime error, never a completed fast return.

NATIVE64_ARCH=arm64 selects comp-arm64.c on a native ARM64 host. Existing test
compiler/runner/flag overrides apply. Hosted jobs use O3/NDEBUG ASan+UBSan and
ordinary leak detection. Controls accept only exact semantic failures, never a
compiler error, sanitizer report, crash or timeout. See doc/native64-series0130.md
for the private API boundaries, ownership contract and measured counts.

`python3 utils/tests/dis64-numeric-fault.py` adds actual Jit-constructor rollback:
every native allocation fails once by return and once through the real VM error
stack; mmap and mprotect each fail by return and error. Real host mappings and
permission changes are observed through linker wrappers. `--control lease` and
`--control mapping` must fail their exact ownership baselines. No guest execution
or collector runs in this fault fixture. See doc/native64-series0131.md.

## Executing a checked Sys LOAD

The private console profile accepts one or both of these scalar imports in
either ordinal order.

| Symbol | Result | Arguments |
| --- | --- | --- |
| millisec | int | none |
| sleep | int | one int, currently the constant zero |

For example, compile this source with the host Limbo compiler.

```limbo
implement Clock;
include "sys.m";
Clock: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    status := sys->sleep(0);
}
```

```sh
limbo -64 -I module -o clock.dis64 clock.b
emu -r /path/to/root -c 1 -d /clock.dis64 -P '$Sys'
```

Place the output at the console root's `clock.dis64`. The loader resolves each
record's symbol and structural signature against trusted Sys exports, builds
the import group in the recorded order, and executes ILOAD through the VM.
The checker verifies each MFRAME/MCALL ordinal and requires the argument setup
matching that entry. A source may contain 1 through 32 calls. The compiler
preserves their order.

The test command below runs independently serialized reversed groups,
sleep-only groups, compiler-produced sources, invalid ordinals including
negative values, and signature/argument refusals in C and JIT modes.

```sh
python3 utils/tests/dis64-pair-boot.py --sys-clock --limbo /path/to/limbo \
    --root /path/to/root --emu /path/to/emu
```

The full AWM Sys group additionally needs fildes, file2chan, fprint, open, pctl
and sprint. Its managed and variadic arguments remain outside this profile.
