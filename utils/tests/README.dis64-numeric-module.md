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

## Cross execution on Linux

The complete console and the ownership fixtures can run through QEMU user mode.
Use a runner reservation large enough to retain addresses above 4 GiB.
For QEMU 10.0.13, the following runner avoids an early PIE/brk collision
observed without an explicit reservation.

```sh
export NATIVE64_ARCH=arm64
export CC=aarch64-linux-gnu-gcc
export NATIVE64_RUNNER='qemu-aarch64 -R 0x10000000000 -L /usr/aarch64-linux-gnu'
export NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all'
python3 utils/tests/dis64-numeric-gc.py --managed --limbo /path/to/host/limbo
python3 utils/tests/dis64-numeric-gc.py --calls
python3 utils/tests/dis64-numeric-gc.py --provider
CC="$CC" CFLAGS="$NATIVE64_CFLAGS" python3 libinterp/tests/sys-pctl.py
```

For RV64, select `NATIVE64_ARCH=riscv64`, `CC=riscv64-linux-gnu-gcc` and
`qemu-riscv64 -R 0x10000000000 -L /usr/riscv64-linux-gnu`. The RV64 CI
flags currently omit UBSan. Native host tools, including Limbo, run directly
on the build host. The target fixture binaries run through
`NATIVE64_RUNNER`, including the emu/os pctl tests in both WORD models.

On 2026-11-11, GCC 14.2.0 and QEMU 10.0.13 built and executed both complete
console targets. The paired boot corpus covered compiled libraries, shared
MP, multiple dependencies, scalar Sys LOAD groups, managed String pairs and
admission failures. The scalar corpus completed 38 processes and 1096
in-program checks on each target.

The managed GC fixture requires four native provider completions and four
native paired-image completions on ARM64 and AMD64. RV64 requires two completions in each counter for HEADP after the
owner-path addition. Pointer CONS continues through the C handler. Both target runs cover 483 collection epochs and 19
constructor allocation refusals. Full AWM startup and hardware acceptance
remain separate milestones.
