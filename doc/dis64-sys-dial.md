# Sys dial in the private Dis64 file trial

[Русский](dis64-sys-dial.ru.md). See the [Connection I/O extension](dis64-sys-connection.md) for field access, status guards and data exchange. This extends the existing Inferno VM and JIT;
it does not import DisVM or implement RPC proxies.

`limbo -64l` now accepts a bounded void-entry module that loads `$Sys` and
performs complete `dial(string, string): (int, Connection)` calls:

```limbo
implement DialTrial;
include "sys.m";
DialTrial: module { init: fn(); };
init()
{
    sys := load Sys Sys->PATH;
    (status, connection) := sys->dial("tcp!127.0.0.1!12345", nil);
}
```

Start a TCP listener on that port, compile with
`limbo -I module -64l -o /build/dial.dis64 dial.b`, then run:

```sh
/build/emu/Linux/o.emu-g -v -c 0 -r /build \
    -d /dial.dis64 -P '$Sys' -X dis64-managed-trial
```

Repeat with `-c 1` to request JIT. A completed run does not establish that every
operation executed machine code. The listener observes the connection and EOF
when the module retires. The console's normal namespace setup binds the host IP
device under `/net`. This example closes its result on retirement and prints
nothing. Failed native dial returns status -1 and an empty Connection.

The current compiler proof accepts up to 32 calls, direct String literals/nil,
repeated assignment to the same `(status, connection)` locals, and explicit Sys
reloads between calls. The tuple is rooted and its C result is checked by the
owning tests. Reading its fields/status in subsequent Limbo expressions, mixing
other imports, branches and arbitrary ADTs remain outside this bounded producer.
Such extra effects are rejected, never silently discarded. This is not general
shell/AWM admission or a finished Android GUI client.

## Native layout and ownership

| Region | Layout |
| --- | --- |
| Caller | 112 bytes, exact map `00 dc` |
| Caller argument roots | String FP64 and FP72 |
| Caller result | Word status FP80, unmapped padding FP84..87 |
| Caller Connection roots | FD FP88, FD FP96, String FP104 |
| Native callee | 80 bytes, exact map `00 c0`, String arguments 64/72 |
| Borrowed result address | Full eight-byte address in native frame offset 32 |

`native64diallea8step` accepts only that exact caller/result/callee geometry.
The old Word/FD result binder is unchanged for its admitted callers. Loader
validation checks the entire call sequence and private structural tuple tree
before allocating. The runtime resolves the real generated `dial` entry and
legacy consistency hash, then uses ordinary LOAD/linkmod/MFRAME/MCALL. LOAD does
not invoke init. The fixed native Sys startup profile now selects 23 exports;
the separate String/Word/FD/array file group still admits its original 14 schemas.

The trial uses private tuple witness `0x77` alongside its existing private
ref/ADT/String witnesses. This assigns no public managed kind or shared ABI.

The real C wrapper captures the native frame, output address and previous
Connection before conversion or release. A new private, externally rooted
construction owner retains both partial FDs and the directory String through
nested allocation and cleanup callbacks. It transfers all three references into
the caller result before removing the construction root. Failure closes each
new raw/managed descriptor, removes the temporary root and preserves the previous
managed tuple. Status may already be -1 after the native call; this is not an
atomic transaction over all 32 result bytes. Native refusal clears the old tuple.

The temporary owner fixes a GC gap in `makeconnection`: a first adopted FD was
previously held only on the C stack when a later allocation's cleanup released
the VM. The shared C helper also serves announce/listen; those functions have no
new Dis64 file admission in this change. The temporary root follows Inferno's
existing exclusion/ptradd contract; it is not independent concurrent-GC acceptance
for FlashVM's host adapter.

## Reproduction

```sh
python3 utils/tests/dis64-sys-fixed-gc.py --limbo /host/tools/limbo
python3 utils/tests/dis64-sys-dial.py --limbo /host/tools/limbo \
    --root /build --emu /build/emu/Linux/o.emu-g
```

The owning gate runs real VM instructions, linker, collector, generated frames,
Sys_dial and FD cleanup against a controlled native boundary: 16 C/JIT cases,
16 stage/FD/String allocation failures, malformed tuple/root/address/signature
refusals and partial-construction rollback. It checks status, untouched padding,
replacement, previous owners and resource balance. Deliberate removal of tuple
root/address checks must fail admission controls. The full-console gate opens
70 real loopback TCP connections with C/JIT requests, checks EOF after replacement
or retirement and refuses malformed files before scheduling. It is also part of
the existing paired-console gate.
