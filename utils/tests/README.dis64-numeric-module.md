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

The private console profile accepts a group of one through three scalar
imports in any ordinal order.

| Symbol | Result | Arguments |
| --- | --- | --- |
| millisec | int | none |
| sleep | int | one int |
| dup | int | two ints |

For example, compile this source with the host Limbo compiler.

```limbo
implement Clock;
include "sys.m";
Clock: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    value := -1;
    status := sys->dup(value, value);
    value += 4;
    status += value;
    status = sys->dup(status, value);
    value = sys->sleep(0);
    status = sys->dup(-1, value);
    status = sys->sleep(value);
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
preserves their order. The producer tracks up to three initialized local integers and
the latest returned integer. Arguments may use any initialized value or a literal.
Legacy module, prepared-frame and result slots are checked against the actual
Limbo frame extent before remapping, so their placement may vary.

Each immediate initializer or argument must fit signed30, from -536870912
through 536870911. Values returned by Sys retain their full 32-bit integer
representation. The caller stores persistent locals at offsets 64, 68 and 76 and the latest
result at offset 72. Each local has an independent initialization bit. Prepared callee argument cells are at offsets 64 and 68,
with the full native result address at offset 32. These offsets belong to
different frames. A call which replaces the saved local also copies its new
result into the saved caller cell. Positive sleep periods
use the normal Sys wait/release/acquire path. Negative periods retain ordinary
Sys behavior. Large positive periods are checked for encoding without executing
the corresponding long waits in the test corpus.

The selected Sys profile now registers 15 entries, including dup, using
Limbo-generated sys.m metadata. The loader checks both the structural wire
signature and the generated native export before publishing a caller.

The test command below runs independently serialized reversed groups,
sleep-only and two-argument calls, all six orders of a three-symbol group,
compiler-produced sources, signed30 encoding boundaries, invalid ordinals
including negative values, initialized-local and result chains, and
signature/count/argument-slot refusals in C and JIT modes. Admission rejects a
read of either scalar cell before its initializer or completed call. Prefix,
prepared-frame pointer and unrecognized scalar offsets are excluded.

A recognized Sys interface with unsupported scalar body or argument state
produces the diagnostic `unsupported checked Sys body or uninitialized scalar
argument`. More than three persistent locals, branches and loops need broader
scalar lowering. Linear integer assignments and `+`, `-`, `*`, `/`, `%`,
`&`, `|`, `^`, `<<` and `>>` are supported between calls using the existing
scalar instruction lowering. Reads must refer to an initialized persistent local
or the latest completed result. The checker proves those facts independently
of the compiler. Arithmetic retains ordinary 32-bit VM behavior, including
overflow and runtime zero-divide exceptions. Binary immediate middle operands
must fit signed16.

For example, all three integers remain available across subsequent calls:

```limbo
    one := -1;
    two := -2;
    three := -3;
    status := sys->dup(one, two);
    one = sys->sleep(0);
    two = sys->dup(three, three);
    three += two;
    status = sys->dup(one, three);
```

The latest-result cell is transient: an otherwise unpreserved call result is
replaced by the next call. Assign a result to an already initialized local to
preserve it. The compiler emits a checked copy to that local after MCALL; all
three destination cells are covered by execution tests. Local cells cannot
alias the native module slot, prepared-frame pointer, or result-address field.
The module and frame pointer maps and the 80-byte frame size stay unchanged.

A body may contain at most 64 scalar instructions, including initializers,
assignments, arithmetic and compiler-inserted copies of call results into the
saved local. This is separate from the 32-call limit. A body exceeding either
limit is rejected before publication. Tests cover both exact boundaries,
combined copy/arithmetic budgets, shifts and bit operations, overflow,
zero-divide execution, and malformed operands in independently serialized
images as well as compiler-produced sources.

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

The managed GC fixture requires four native provider completions and 132
native paired-image completions on ARM64, AMD64 and RV64. RV64 now includes
both pointer HEAD and pointer CONS. All three target runs cover 25083 collection epochs, 264 paired calls and 19
constructor allocation refusals. Full AWM startup and hardware acceptance
remain separate milestones.


The managed file corpus also executes repeated filesystem LOADs through the
ordinary module cache and linker. The controlled-I/O/production-GC fixture checks
56 cases, 832 LOADs and 400 additional native completions: distinct instances
reuse the same Module/JIT for unchanged files and replace it after identity
changes. Malformed replacement files, short reads, open failures and signing
refusals retire without leaked owners. See `doc/dis64-managed-string-admission.md`
for the source example and remaining profile limits.
