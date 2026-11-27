# Forwarding calls compiled from Limbo

[Русская версия](dis64-typed-forwarding.ru.md).

The previous [active-frame call gate](dis64-typed-calls.md) supplied its
intermediary caller in C. This step lowers the complete forwarding wrapper from
checked compiler IR. It supports the canonical shape `return helper(args)`:
one FRAME, every declared argument in order, an inherited result address,
one CALL to a validated local function entry and one RET. The helper can itself
be another accepted wrapper. Cycles and more than64 active functions refuse.
This is ordinary CALL/RET, not frame-eliminating tail-call optimization.

## Example

```limbo
implement Forward;
Forward: module {
    count: fn(n:int):int;
    relay: fn(n:int):int;
    chain: fn(n:int):int;
};
count(n:int):int {
    total:=0;
    while(n>0){total+=n;n--;}
    return total;
}
relay(n:int):int { return count(n); }
chain(n:int):int { return relay(n); }
```

Run the complete source/native gate with host Limbo from this branch:

```sh
python3 utils/tests/native64-typed-tail.py --limbo /path/to/host/limbo
python3 utils/tests/dis64-typed-tail-unit.py
```

For ARM64/Linux, select `CC=aarch64-linux-gnu-gcc`, `NATIVE64_ARCH=arm64` and
`NATIVE64_RUNNER='qemu-aarch64 -L /usr/aarch64-linux-gnu'` for the native gate.
The IR unit gate uses the same CC/runner without needing NATIVE64_ARCH.
Both gates are included in native64-port.py.

The compiler report is opt-in:

```sh
LIMBO_DIS64_TYPED=1 LIMBO_DIS64_TYPED_TAIL=1 \
    /path/to/host/limbo -i -o Forward.dis Forward.b
```

`-i` is the existing Limbo option to disable source inlining. It keeps these
calls visible for the test; it is not a new64-bit switch. The ordinary .dis
output remains classic bytecode. This command does not create a loadable native
module or admit a new Dis64 file profile.

## Relocation and proof

The compiler resolves the exact target entry against its function table and
checks the FRAME descriptor against that callee. Each argument store must match
the declaration's order, exact classic offset, opcode/type and native layout.
Byte/int/fix/big/real and managed-reference arguments retain their widths and
root roles; aggregate copies and varargs remain closed. Every descendant must
lower successfully before a wrapper is accepted. Trusted typechecked source
supplies full type compatibility, not arbitrary external metadata.

Limbo's inherited result expression is LEA through classic FP[16]+0 into the
prepared frame's byte16 address field. The wrapper instead copies all eight
address bytes from native FP[32] into prepared-frame byte32 with MOVL. This does
not execute ordinary Word32 LEA. The pending frame is kept in reserved native
header cell40 and never marked as a managed root. The original result owner
stays live through the normal stack chain until the descendant returns.

This deliberately avoids introducing a generic new address-binding opcode or
changing existing Sys gateways. Binding a fresh local result (`v:=helper(n)`),
performing computation after the call, branches around setup, recursive chains,
void forwarding and module-variable MCALL remain refused here.

## Private report and native bridge

Accepted wrappers use private schema inferno-typed-tail-v1. `tail` contains
`[classic_callee_entry, classic_descriptor_id]`. Leaves retain
inferno-typed-body-v2. Code rows keep six values. The last value64 marks a store
through the one prepared pointer40, including the inherited-address MOVL.
It is separate from v2 result-store width/reference contracts; the existing
leaf materializer rejects it.

`native64typedtailcode` consumes trusted immutable rows and the module builder's
resolved callee Type, entry pointer and native Type ordinal. It checks structural
encoding and publishes no output on refusal. The builder still owns consistent
source/type/target identity and disjoint input/output storage; this function is
not a validator for attacker-provided metadata or a lifetime lease. No heap
allocation or callback occurs during materialization.

## Results and remaining work

18 source functions include six leaf kinds and12 real forwarding wrappers.
Each x64 and ARM64/QEMU run passes288 C/RX, chain depth, growth, quanta and input
cases with scalar canaries and exact managed ownership teardown. String/list
identities transport opaque owned graphs in this call fixture; actual String/
list/Unicode/GC operations retain their separate existing gate. Allocation/OS/
collector boundaries are bounded fixture support, not concurrent GC/scheduling.

A five-argument String/int/big/real/byte case verifies mixed-width source
relocation. Five source refusal cases cover direct/mutual recursion, fresh
local results, post-call arithmetic and branches. Three compiler predicate
controls and two native bridge controls require exact semantic failure, never
crash/timeout/sanitizer failure. Eight native refusals preserve output bytes.

The existing default AWM matrix stays14 accepted/346 refused. With source
inlining disabled, comparison against the previous compiler is17/380 both before
and after this change; no new AWM startup body is claimed. All16 default sources,
376 storage plans and191 existing private-profile images retain legacy bytes.

Only the outer test entry and initial result binding remain constructed in C;
all intermediary forwarding calls now come from Limbo. General source-call
computation, managed module writing/loading and Android GUI startup remain
unfinished. The execution evidence is x64 and ARM64/Linux QEMU, not Android/
Bionic, an AWM run or a ready APK.

Local-result calls and continuations: [dis64-typed-local-results.md](dis64-typed-local-results.md).
