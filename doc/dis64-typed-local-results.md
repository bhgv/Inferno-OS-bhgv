# Local results and computation after native64 CALL

[Русская версия](dis64-typed-local-results.ru.md).

The [forwarding call gate](dis64-typed-forwarding.md) passed an inherited result
address down a source call chain. This gate also binds a fresh typed local field
and lowers ordinary instructions after the call, including branches and loops
confined to that continuation.

## Complete Limbo example

```limbo
implement Local;
Local: module {
    count: fn(n:int):int;
    relay: fn(n:int):int;
    chain: fn(n:int):int;
};
count(n:int):int {
    total:=0;
    while(n>0){total+=n;n--;}
    return total;
}
relay(n:int):int {
    v:=count(n);
    if(n<0)return v-1;
    return v+1;
}
chain(n:int):int {
    v:=relay(n);
    return v+2;
}
```

Run with host Limbo built from this branch:

```sh
python3 utils/tests/native64-typed-local.py --limbo /path/to/host/limbo
python3 utils/tests/dis64-typed-local-unit.py
```

The native gate accepts the usual CC, NATIVE64_ARCH and NATIVE64_RUNNER variables
for ARM64/Linux cross execution. Both gates now run in native64-port.py.
For a private compiler report, use:

```sh
LIMBO_DIS64_TYPED=1 LIMBO_DIS64_TYPED_LOCAL=1 \
    /path/to/host/limbo -i -o Local.dis Local.b
```

The existing `-i` option preserves source calls by disabling inlining. The .dis
output remains classic bytecode. This is still a checked-IR/native construction
experiment, not a managed Dis64 module writer/loader or APK build command.

## Compiler scope

One canonical local call island must start the function. FRAME, every ordered
argument, LEA binding and exact local CALL are validated against the callee's
entry, descriptor, result kind and native storage plans. Descendants must also
lower, with existing cycle/depth bounds. This first local profile requires the
callee and caller result kinds to agree. Results target typed direct FP fields;
MP result bindings, aggregate results and varargs remain closed.

After CALL, the existing typed instruction lowerer handles scalar/reference
moves, arithmetic, length operations, branches and returns. No branch may enter
or revisit the setup island, including its FRAME. Another call in the same body,
computation before the first FRAME, void calls and module-variable MCALL remain
closed. A prepared-argument store or local address binding cannot prove that the
function's own result is defined: every reachable RET still needs a true result
store on every incoming path.

## Full-width binding in a live stack

The new `native64typedlea8step` callback is selected only by a trusted unpublished
Module builder through the existing cstep field. It is not selected by a file
flag, and Module/Frame layouts are unchanged. The normal VM loop invokes it before
ordinary decode/JIT dispatch. Binding runs in C in both C and RX test modes; this
is not a new hardware LEA emitter.

The private LEA retains the existing ISA/address modes. Its unused middle register
stores width OR reference-bit16: byte1, Word4, big/real8 or reference24. The adapter
checks native Word32/ref64 geometry, current-frame descriptor bounds/alignment,
the result field's GC role, the one pending pointer at header40, adjacency to the
caller or the expected new extent, the complete pending extent and its descriptor.
It then decodes and publishes all eight native address bytes into pending byte32.
Ordinary Word32 LEA keeps its original behavior. The existing decoded-JIT guard
continues to reject any attempt to bypass predecode binding for a translated owner.

These are trusted readable VM frames/Types under synchronous VM exclusion. Type
and source metadata stay immutable and live through ordinary stack/Module
ownership. There is no independent lifetime pin, concurrent publication/ABA
protocol or authentication of arbitrary native input. Exact semantic kind and
field provenance remain checked compiler premises; maps alone prove only root role.
The binding callback allocates nothing and acquires no ownership.

## Private report and materializer

Accepted local callers use inferno-typed-local-v1, alongside unchanged v2 leaves
and optional forwarding-v1 rows. The existing `tail` field identifies the callee;
`local` contains `[relative_CALL_pc, native_result_offset, width_or_reference]`.
The six-field code row uses64 for prepared argument stores and128 OR contract for
a local binding. Ordinary result-store contracts stay unchanged.

`native64typedlocalcode` resolves the trusted Type/entry/ordinal supplied by the
unpublished builder, checks the complete encoding before touching output and
rebases continuation branches at their real embedded module positions. Input/
output buffers must be disjoint and immutable during construction. This is not
an arbitrary code validator or a newly assigned public format.

## Validation and limits

Each x64 and ARM64/Linux QEMU run covers288 source successes across six result
kinds, C/RX, nesting, extent growth, quanta and inputs. Caller branches execute
both paths. An additional96 private continuation faults replace the first
continuation instruction with division by zero after a managed result has arrived:
the original outer result stays intact and actual stack teardown releases every
local/argument owner. This is fault injection, not an extra source language feature.

1536 malformed binding probes per architecture check width, root role, offsets,
header fields, pending frame and descriptor state before pointer/result/PC writes,
with zero allocations. Wrong-owner and decoded-JIT bypass checks also pass.
Eight materializer refusals preserve the entire output buffer. Two runtime width/
map omission controls and two compiler result-definition/setup-branch controls
must reach exact semantic failures; crashes/timeouts/sanitizer failures do not count.

The call fixture transports opaque owned reference graphs through String/list
identity bodies; real String/list/Unicode/GC operations retain their separate
gates. Runtime uses UBSan and bounded OS/allocator/collector support, not concurrent
GC or OS scheduling. Compiler/core units pass host ASan/UBSan and ARM64 UBSan.
The existing runtime-ASan unwind limitation remains open.

Ordinary default AWM analysis stays14/346; same -i comparison stays17/380. All16
startup source outputs,376 layouts and191 existing private images retain their
legacy bytes. Both managed file profiles still refuse the mixed local-result
module. General calls, managed module writing/loading and Android GUI startup
remain unfinished; no Android/Bionic device, AWM execution or ready APK is claimed.

Multiple calls, prefix computation and void results: [dis64-typed-multiple-calls.md](dis64-typed-multiple-calls.md).
