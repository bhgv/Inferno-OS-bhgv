# Limbo file clients that load scalar libraries

This page records the0182 baseline. The current compiler also supports
[branches, loops and module nil checks](dis64-client-control-flow.md).

[Русская версия](dis64-scalar-clients.ru.md).

A bounded importing Limbo command can now be compiled to Dis64 and started by the
complete Inferno emulator. Both command and library CODE come from files. The
command executes production ILOAD, namespace admission, structural linking and
MFRAME/MCALL; its instruction array, root maps and import requests are no longer
supplied by a C fixture. Android GUI/AWM remains a separate unfinished milestone.

## Two files and one ordinary interface

`library.m`:

```limbo
Library: module {
    PATH: con "/demo/library.dis64";
    inc: fn(n: int): int;
};
```

`library.b`:

```limbo
implement Library;
include "library.m";
inc(n: int): int { return n+1; }
```

`client.b`:

```limbo
implement Client;
include "library.m";
Client: module { init: fn(); };
init() {
    m := load Library Library->PATH;
    first := m->inc(7);
    m = load Library Library->PATH;
    last := m->inc(first);
}
```

Compile with the new host compiler, put both files under the emulator's `/demo`,
and select the existing explicit experimental startup policy:

```sh
limbo -64b -I . -o library.dis64 library.b
limbo -64c -I . -o client.dis64 client.b
emu -r /path/to/inferno-root -X dis64-calls -c 1 -d /demo/client.dis64
```

Use `-c 0` for C interpretation. This example computes8 and then9; it prints
nothing because this subset imports no Sys interface. `Library` is the interface
type, `Library->PATH` supplies the filename, and `load` becomes a Dis instruction.
Library init is never invoked automatically. A library export named init is an
ordinary callable function, exactly as before.

No `-H -R` C header is needed for this file client. The producer derives structural
requests directly from the compiler's final LDT and `.m` function declarations.
It preserves the compiler's selected import ordinals and writes every record in
that narrowed LDT. Unused interface declarations omitted by normal Limbo narrowing
are not added back. This differs from the optional embedding header's complete
`.m` request table, without changing either contract. Every serialized request is
matched; no32-bit interface hash is invented or substituted for signaturev1.

## Currently admitted command

The compiler accepts one void, argument-free init function and one constant ASCII
library path. The same module local may be reloaded. Imported signatures use
void/int/big/real; arguments are already computed scalar locals or Word immediates.
Call results can feed later calls or scalar computations. Providers retain the
existing whole-library scalar DAG/leaf CFG support.

The client itself is linear: supported operations are Word/Big moves and integer
arithmetic/bit operations, shifts, Int→Big/Real conversions and Real moves.
Branches, loops, additional functions, module globals, reference arguments/results,
heap operations, exception handlers and dynamic/multiple dependency paths refuse.
Prepared-frame arguments must be moves; an arithmetic expression fused directly
into an argument store is outside this first producer. Compute a local that stays
live across the call when that occurs. Normal caller-owned scalar result slots are
supported; inherited result addresses are outside an argument-free void root.

Producer limits are128 original IR instructions and4096 final frame bytes; raw
proof additionally bounds256 serialized instructions,64 imports/parameters and64
numeric local spans. Names are nonempty ASCII up to255 bytes and the path is1–1023
ASCII bytes, excluding builtin `$...` paths. Large numeric constants that require
extra MP storage remain refused; use ordinary locals and supported conversions.

Missing/incompatible libraries make load produce nil/H; the following module
call raises the ordinary `module not loaded` error. Nil-check branches/handlers
are not admitted by this linear subset yet. `$Sys` and `$self` keep their existing
separate paths; this change does not grant a generic native builtin interface.

## Independent file proof and ownership

The five sections use existing Module/Code/Types/Imports/Strings framing and
signaturev1. The private Inferno admission shape fixes a data-free scalar entry
frame, a24-byte mapped MP and one structural import table. String-pool IDs are
0 command name,1 path,2 interface name and3+ordinal function names. The constructor
hydrates the one owned filename String from that checked path; MP slots0/8 start
H and slot16 holds that String. This is an Inferno bounded startup convention,
not a newly agreed generic managed DATA initializer or FlashVM execution claim.

The allocation-free checker decodes the entire image and proves exact root roles,
all loads and prepared call groups, parameter alignment/kinds, ordinal identity,
result-address tail/copy and definite scalar initialization. Numeric spans cannot
overlap or change kind. There are no hidden branches or accesses to native Frame64
headers. Every prepared frame must reach its matching call in the linear code;
provider faults follow normal unwinding. Refusal leaves the output view untouched.
The compiler runs the same independent raw proof before writing any file payload.

Only then does the runtime allocate Types, code, MP, filename and copied requests.
The MP is externally rooted throughout construction and compiler/JIT callbacks.
Every protected failure retires its acquired pin and all partial owners. Success
has no leftover temporary pin: the caller must publish/register before another
collecting VM operation, as parsemod immediately does under exclusion. Construction
alone grants no file policy; namespace signing/admission remains in parsemod.
The cached command Module and each linked instance use ordinary MP clone rules.
Libraries still receive fresh uncached reads/admission/whole-provider proof.

The file caller receives a trusted native reader callback only from the checked
constructor. Its library JIT request follows host cflag at LOAD, with dontcompile
checked again after I/O. Trusted C embedding tables keep their explicit JIT request.
The private native-only bit29 identifies this constructor; it is never decoded
from file flags. No public wire/ISA/signature ID, Frame64/Module/Modlink layout or
vendor pin changes in this batch.

## Reproduce and evidence

```sh
python3 utils/tests/dis64-scalar-client-producer.py --limbo /host/limbo
python3 utils/tests/dis64-scalar-client-proof.py --limbo /host/limbo
python3 utils/tests/dis64-scalar-client.py --limbo /host/limbo
python3 utils/tests/dis64-scalar-client-boot.py \
    --limbo /host/limbo --emu /root/emu/Linux/o.emu-g --root /root
```

Use existing CC/NATIVE64_ARCH/NATIVE64_RUNNER settings for ARM64/Linux QEMU.
The raw checker has initialized/result/map omission controls; malformed inputs
never execute. Constructor ASan/UBSan uses `--materialize-only` and sanitizer
NATIVE64_CFLAGS. The known runtime-ASan instruction/unwind limitation remains.

Seventeen source pairs pass1632 scheduler roots per target across caller/provider
C/RX, stack geometries, quanta1/3/1000 and open/admission/JIT-disable outcomes.
The fixture checks exact scalar bits and genuine accepted RX. It poisons the
client image before startup and injects every client read/constructor allocation
failure. Namespace/GC support there remains bounded.

The rebuilt complete consoles separately run34 C/RX file-client OS roots each
using actual namespace, heap, MP cloning and scheduler. Validly typed wrong-result
providers trigger the command's own integer guard; signature mismatches and missing
providers retire normally with `module not loaded`. Default startup and malformed
client maps refuse before execution. These are positive full-OS importing file
runs, unlike the earlier native-authored caller fixture. They do not establish
concurrent GC/filesystem stress, general managed graphs or a ready Android GUI APK.

[INFERNO-0182 report](dis64-inferno-scalar-client-report.md).
