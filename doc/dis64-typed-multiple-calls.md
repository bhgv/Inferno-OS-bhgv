# Multiple typed calls in a native64 function

[Русская версия](dis64-typed-multiple-calls.ru.md).

This extends [fresh local results](dis64-typed-local-results.md) to complete
functions with prefix/intermediate computation, multiple local call sites, void
calls, inherited results and loops that reenter a complete preparation sequence.

## Complete example

```limbo
implement Calls;
Calls: module {
    count: fn(n:int):int;
    sum: fn(n:int):int;
    repeated: fn(n:int):int;
};
count(n:int):int {
    total:=0;
    while(n>0){total+=n;n--;}
    return total;
}
sum(n:int):int {
    x:=n+1;
    first:=count(x);
    second:=count(n);
    return first+second;
}
repeated(n:int):int {
    saved:=sum(n);
    total:=0;
    while(n>0){v:=count(n);total+=v;n--;}
    return saved+total;
}
```

The first result remains a normal typed local while another callee runs. The
loop returns to a complete setup sequence, never into its argument stores.

```sh
python3 utils/tests/native64-typed-multi.py --limbo /path/to/host/limbo
python3 utils/tests/dis64-typed-multi-unit.py
```

Use the existing CC/NATIVE64_ARCH/NATIVE64_RUNNER settings for ARM64/Linux QEMU.
Both gates run in native64-port.py. To inspect the private compiler report:

```sh
LIMBO_DIS64_TYPED=1 LIMBO_DIS64_TYPED_MULTI=1 \
    /path/to/host/limbo -i -o Calls.dis Calls.b
```

The existing `-i` disables source inlining so the calls remain visible. Calls.dis
is still classic bytecode. This is not a command to produce an admitted managed
Dis64 image or an Android APK.

## Compiler proof and cache

Each canonical island owns FRAME, ordered scalar/reference argument moves, an
optional local/inherited result binding, and its matching CALL. The target is
an exact local function entry with the matching descriptor. Void callees have
no result binding. A local result uses the callee's kind, independently of the
caller's return kind, so typed computation/conversion can follow the call.

Ordinary typed instructions may precede, follow or separate islands. Every
branch stays within its function and must avoid all island interiors; targeting
the island's FRAME is allowed. No branch can escape from inside setup, and nested
argument preparation is not accepted. Precompute an argument expression into a
local when the compiler would otherwise emit arithmetic through the pending
pointer. Aggregate argument copies, generic/variadic functions, exception
handlers and module-variable MCALL remain closed.

A per-root context indexes instructions and function entries. Each reachable
function plan is built once and shared by repeated sites. Active cycles refuse;
cache heights enforce the complete path bound, including cached subgraphs.
Limits are4096 function entries,65536 instructions,64 functions per call path
and32 MiB for accumulated native frame/data extents plus layout leaves. Owned
cache plans are released after success/refusal; only the root output is moved
out. Nothing mutates the compiler's source IR or ordinary bytecode.

A successful inherited CALL establishes the function's return value conditionally
on normal return, using private marker32. Prepared argument moves64 and local
address bindings128|contract do not establish it. Every reachable RET still
requires a true result definition on every incoming path.

## Private native construction

Accepted callers use inferno-typed-multi-v1. The `calls` array contains:

| Position | Meaning |
| --- | --- |
|0|Relative FRAME PC|
|1|Relative CALL PC|
|2|Classic callee entry PC|
|3|Classic callee descriptor ID|
|4|Native local result offset,32 if inherited,0 if void|
|5|Result byte width OR reference-bit16,0 if void|
|6|Inherited-result flag|

Code rows retain the six-field private step representation. Leaves remain v2.
The builder resolves each callee's native Type, entry pointer and Type ordinal.
`native64typedmulticode` checks sorted disjoint sites, physical argument spans,
result/address encodings and branch ownership before touching any output. Branch
membership uses the sorted site table. Input/output storage must be valid and
disjoint; full semantic field/type/target identity remains trusted compiler and
builder metadata, not an arbitrary-input validation claim.

The existing trusted cstep profile binds fresh local addresses before decode/JIT,
with its Word32/ref64 width/map/live-stack checks. Inherited addresses move all
eight bytes through FP32. Header40 holds the one pending pointer, reused only
between complete sites. No allocation/callback occurs during materialization.
Module/Frame layouts, ordinary LEA and public Dis64/ISA IDs do not change.
The binding step still runs in C in both C/RX modes; no hardware LEA emitter or
concurrent lifetime pin/ABA protocol is introduced.

The runtime fixture creates a private complete native module from the report.
Its shared MP descriptor initializes the nil root with heapz; the ordinary Word
`out` field observes void calls. This is trusted fixture initialization, not a
general data writer/loader. The outer entry/result binding remains constructed in C.

## Evidence and remaining scope

Each x64 and ARM64/Linux QEMU run passes480 cases:384 source successes and96
private faults after the final managed call.23 source functions contain32 sites.
They cover six value kinds, void calls, inherited results, preserved first values,
second nil results, prefix computation, nested calls, loop reentry, growth and
quanta. Managed fault teardown leaves the outer result intact and drains owners.

1728 malformed binding probes per architecture preserve pointer/result/PC with
no allocation. Twelve structural refusals preserve all output bytes. Four
compiler predicate controls, two native width/interior controls and two existing
binding width/map controls require exact semantic failure. Four corrupted reports
also refuse. Separate source checks cover mixed return kinds, a shared40-function
DAG, the64-function boundary and mutual-cycle refusal. Compiler/core host
ASan/UBSan and ARM64 core/runtime UBSan pass. Runtime-ASan unwind remains open.

String/list identities transport opaque owned graphs in this bounded call fixture;
actual String/list/Unicode/GC operations retain their separate gate. OS/allocator/
collector boundaries are fixture support, not concurrent GC or OS scheduling.

Default AWM analysis remains14/346; equal -i comparison remains17/380. All16
startup outputs/376 layouts and191 previous private images retain their bytes.
Both existing file profiles still refuse the mixed managed module. General module
writer/loader admission, MCALL/heap operations and GUI startup remain unfinished;
no Android/Bionic device, AWM run or ready APK is claimed.

Owned modules, export signatures and cross-module execution: [dis64-typed-modules.md](dis64-typed-modules.md).
