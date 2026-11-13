# Using a Sys Connection in Dis64

[Русская версия](dis64-sys-connection.ru.md).

The private `limbo -64l` file profile now supports a useful TCP client: check the
status returned by dial, use Connection.dfd/cfd/dir, write/read byte buffers,
convert only the received prefix and keep a buffer across another dial. It uses
the existing Inferno VM and JIT, without importing DisVM or proxy machinery.

Save this as `connection.b` and start a loopback server on port 12345 that reads
`ping`, sends `pong\n`, then reads the client's echo:

```limbo
implement ConnectionTrial;
include "sys.m";
ConnectionTrial: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    (status, c) := sys->dial("tcp!127.0.0.1!12345", nil);
    if(status < 0)
        return;
    b := array of byte "ping";
    n := sys->write(c.dfd, b, len b);
    b = array[16] of byte;
    n = sys->read(c.dfd, b, len b);
    if(n < 0)
        return;
    text := string b[:n];
    r := sys->werrstr(text);
    b = array of byte text;
    n = sys->write(c.dfd, b, len b);
}
```

```sh
limbo -I module -64l -o /build/connection.dis64 connection.b
/build/emu/Linux/o.emu-g -v -c 0 -r /build \
    -d /connection.dis64 -P '$Sys' -X dis64-managed-trial
```

Repeat with `-c 1` to request JIT. `werrstr` stores the text; it does not print.
Read/write can return a short count. This bounded example echoes one read, not a
complete framed network message, and assumes its small write succeeds. Production
protocol code needs its own write loop, framing, EOF policy and UTF-8 boundaries.
The test server accepts a nonempty echoed prefix; controlled C tests separately
force short/error reads and check the exact converted bytes.

## Supported source and native frame

One import group may contain dial, read, write, iounit and werrstr in any import
order. The whole group is resolved against generated Sys metadata. The source
proof accepts linear complete calls, Word operations and forward conditional
returns such as `if(status < 0) return`. It tracks a single current Connection,
byte array and converted String, plus two persistent Word locals. Connection
fields can be passed directly: `sys->iounit(c.cfd)`, `sys->werrstr(c.dir)` and
`array of byte c.dir` use the control FD and directory with their actual types.

General branches/loops, copied Connection ADTs, field mutation and arbitrary
aliases remain refused. A temporary slice such as `string b[:n]` ends this
profile's access to the old array local; replace it with a new array before
using that local again. This preserves the existing single-current-array bound.
The previous dial-only profile and its byte images are unchanged.

| Caller offset | Meaning |
| --- | --- |
| 64 / 72 | String argument roots |
| 80 | Dial status Word |
| 84..87 | Unmapped tuple padding |
| 88 / 96 / 104 | Connection data FD / control FD / directory roots |
| 112 / 120 | Byte-array / converted-String roots |
| 128 | Scalar native return Word |
| 132 / 136 | Persistent Word locals |

The caller is 144 bytes with exact map `00 df 00`. Its third serialized type is
the checked byte descriptor, adopted as canonical Tbyte before JIT construction.
The full-pointer result binder distinguishes the 32-byte dial tuple at FP80
from a four-byte scalar result at FP128. Both address and exact callee geometry
are checked. Scalar returns cannot overwrite the saved Word at FP132.

The loader recomputes definite-initialization facts for status, Connection,
array, String and Word locals. A scalar call cannot initialize Connection fields;
a pointer store cannot initialize an integer. Branches may not enter call
preparation or skip required definitions. Unknown methods, malformed root maps,
equal-width wrong-kind uses and incomplete calls fail before allocation.
No new public format fields, managed IDs or opcodes are assigned.

## Validation and Android scope

```sh
python3 utils/tests/dis64-sys-fixed-gc.py --limbo /host/tools/limbo
python3 utils/tests/dis64-sys-connection.py --limbo /host/tools/limbo \
    --root /build --emu /build/emu/Linux/o.emu-g
python3 utils/tests/awm-native64-preflight.py --limbo /host/tools/limbo
```

The owning gate adds 24 C/JIT/GC executions, 39 malformed-image refusals and
18 native/5 heap constructor rollback refusals. Six further allocation failures
with a live Connection preserve its tuple while array/String construction unwinds. Three active omissions prove
that Connection initialization, FD-versus-Word and mixed caller root checks are
necessary. The existing fixed Sys and dial tests still run. The console gate
performs eight real TCP connections, byte exchange, UTF-8 requests, read-prefix
echoes and replacement, plus two failed-dial guards, in C/JIT-requested modes.
It is also included in the paired-console gate.

Android ARM64 remains the first application target. These console checks cover
AMD64 and Linux ARM64 under QEMU, not Android/Bionic GUI/device acceptance. The
AWM preflight now checks both ordinary scalar `-64` and private managed `-64l`:
all sixteen startup modules still require broader managed compilation/linkage,
including argument-bearing entries, library bodies and shell exception tables.
A working bounded network client does not close that general AWM boundary.
