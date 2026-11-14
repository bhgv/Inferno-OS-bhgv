# File-client branches, loops and module nil checks

[Русская версия](dis64-client-control-flow.ru.md).

The scalar importing-file client now supports control flow around loads and calls.
One argument-free void init, one library path/table and scalar signatures remain
the admission boundary. Ordinary AWM init arguments and general managed graphs
are still separate work. See the [file-client tutorial](dis64-scalar-clients.md)
for interface/provider files and build commands.

## Handle a missing library and call in a loop

Using the previous Library.inc interface:

```limbo
implement Client;
include "library.m";
Client: module { init: fn(); };
init() {
    m := load Library Library->PATH;
    if(m == nil)
        return;
    n := 0;
    while(n < 5) {
        n = m->inc(n);
    }
    if(n != 5) {
        zero := 0;
        bad := 1/zero;
    }
}
```

Compile/start as before:

```sh
limbo -64b -I . -o library.dis64 library.b
limbo -64c -I . -o client.dis64 client.b
emu -r /path/to/inferno-root -X dis64-calls -c 1 -d /demo/client.dis64
```

If the provider is absent, the nil check returns normally. If inc adds one, the
loop ends at5. A validly typed implementation returning the wrong values trips
the command's own result check. No Sys printing or automatic provider init is
introduced. Existing signing/admission and complete signature matching still run
before a successful load can produce a linked instance.

Supported client control includes Word/Big/Real comparisons, if/else, backward
branches, loops, break/continue and early returns. A load may occur again inside
a loop. Equality/inequality between the one module local and nil is supported
in either source operand order. Other reference comparisons remain refused.
The existing limits of128 source instructions,256 serialized instructions,
4096 frame bytes and64 numeric spans/imports/parameters are unchanged.

## Native-width module comparisons

Classic Limbo IR uses Word comparisons for module references. In this selected
client mode the compiler derives the module/nil roles from verified declarations
and lowers them to the existing eight-byte BEQL/BNEL equality instructions. It
normalizes operands to the already owned MP0 module root and readonly MP8 H slot.
No new opcode, signature kind, section, native pointer payload or layout is added.

Raw proof grants this special reference-read permission only to that exact
long-equality shape. A Word comparison of the same managed cells refuses, as do
other MP cells. Ordinary Big/Real comparisons retain their independent numeric
kinds. The tests include Big values above32 bits and negative Real comparisons
whose bitwise integer ordering would choose the wrong path.

## CFG proof before construction

The compiler remaps every branch target after inserting native result-tail copies.
A call island consists of MFRAME, its exact arguments, optional result binding,
matching MCALL and result copy. It is one atomic analysis node: a branch may
enter its start or skip it, but cannot land on an argument, result binding,
MCALL or result copy. Every instruction is still decoded and role-checked.

Scalar spans are declared from all checked instructions/signatures and keep one
non-overlapping kind. The checker then computes definite assignment separately:
reads are requirements, writes/normal call results are definitions. Its two-word
mask handles all64 spans. An additional fact records whether a LOAD attempt
precedes a call on every incoming path; it does not pretend a load cannot fail.

Entry starts with no numeric definitions or load attempt. Joins intersect all
predecessor outputs. A descending worklist reaches a fixed point through loops;
a definition from a later iteration cannot initialize the first iteration.
Read requirements are checked only after convergence. All node boundaries must
be structurally reachable, targets must stay inside the command and the last
instruction remains EXIT. Validation work has an explicit132*instruction-count
pop bound; program termination itself is not proved.

This analysis allocates no native heap/VM objects and publishes its borrowed view
only after complete success. Its temporary integer facts contain no live REG
snapshot, runtime pointer authority or persistent JIT metadata. Construction,
external-root cleanup, namespace release/acquire, GC maps, file policy and library
proof retain their existing contracts.

## Verification

```sh
python3 utils/tests/dis64-scalar-client.py --cfg --limbo /host/limbo
python3 utils/tests/dis64-scalar-client-boot.py --cfg \
    --limbo /host/limbo --emu /root/emu/Linux/o.emu-g --root /root
python3 utils/tests/dis64-scalar-client-cfg-proof.py --limbo /host/limbo
for control in join load boundary high; do
    python3 utils/tests/dis64-scalar-client-cfg-proof.py \
        --limbo /host/limbo --control "$control"
done
```

Use the existing ARM64/Linux QEMU compiler/runner variables. Fifteen source pairs
exercise nil guards, both diamond arms, early exit, zero/one/many loop iterations,
reloads, break/continue and full-width Big/Real comparisons. Each target passes1440
scheduler roots with all caller/provider C/RX choices, stack geometries, quanta,
I/O/admission refusal and JIT-disable outcomes. Constructor-only ASan/UBSan covers
the same15 pairs; it does not execute VM instructions under ASan.

Both complete consoles separately pass60 C/RX OS roots, including graceful returns
for every absent provider, plus two valid wrong-result provider controls. Twenty-one
independent raw images cover missing definitions at joins/first loop entry,
setup-interior targets, native-width nil roles, upper-half assignment masks and
64/65-span boundaries. Four omission controls must produce semantic exit10;
malformed instructions never execute. The earlier28-image/three-control corpus,
17 linear clients/1632 roots and34 full-OS roots remain compatible.

The previous17 importing Dis64 outputs are byte-identical to the previous compiler.
All15 new sources retain their classic output, and the AWM body baseline remains
14 accepted/346 refused with classic/traced bytes unchanged. These are source
compatibility checks, not AWM execution. No Android/Bionic GUI APK, new collector/
filesystem concurrency proof or FlashVM signal/debugger implementation is claimed.

[INFERNO-0183 report](dis64-inferno-client-cfg-report.md).
