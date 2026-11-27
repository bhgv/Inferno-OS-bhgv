# Arithmetic and conversions in a Dis64 file client

[Русская версия](dis64-client-numeric.ru.md).

The importing client can now compute with `real` and convert between `int`, `big`
and `real` before and after file-module calls. This extends the existing
[client CFG](dis64-client-control-flow.md), with the same argument-free void entry,
one library/interface/path and scalar-only import signatures. It does not provide
ordinary AWM context/list arguments or general managed module compilation.

## Complete example

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
    if(m == nil)
        return;
    first := m->inc(7);
    a := 3;
    b := 2;
    x := real a;
    y := real b;
    fraction := x/y;
    rounded := int fraction;
    argument := first+rounded;
    last := m->inc(argument);
    if(last != 11) {
        zero := 0;
        bad := 1/zero;
    }
    keepx := x;
    keepy := y;
    keepfraction := fraction;
}
```

Create `/demo` under the emulator root, then:

```sh
limbo -64b -I . -o /path/to/root/demo/library.dis64 library.b
limbo -64c -I . -o /path/to/root/demo/client.dis64 client.b
emu -r /path/to/root -X dis64-calls -c 1 -d /demo/client.dis64
```

Use `-c 0` for interpreter execution. This example returns silently; the conditional
integer divide catches an incorrect result. `int fraction` rounds 1.5 to2 in the
existing VM, so the result is 8+2+1. There is no new Sys formatter dependency.

The selected writer still reserves MP for the library roots/path. Build floating
values from scalar locals, as above; arbitrary real/Big constant DATA records
remain outside this file profile. Keep values of different kinds in distinct
live storage: this profile rejects a reused span interpreted first as `big` and
then as `real`, even though both occupy eight bytes. The final copies in the
example keep those locals live. Arguments to module calls should be prepared
in locals rather than fused expressions in the prepared-frame island.

## Supported operations and runtime behavior

| Operation | Input | Output |
| --- | --- | --- |
| `+`, `-`, `*`, `/` | Real, Real | Real |
| Unary `-` | Real | Real |
| `big word`, `real word` | Int | Big, Real |
| `int wide`, `real wide` | Big | Int, Real |
| `int fraction`, `big fraction` | Real | Int, Big |

Both implicit destination-as-left-operand and explicit three-operand arithmetic
are checked. Compound assignment and arithmetic on a returned Real work through
the existing C handlers and JIT. A Big returned by a file provider can feed a Real
conversion and a later Big call. No automatic library `init` call is introduced.

The VM's Real-to-integer conversions round halves away from zero: 1.5 becomes2,
-1.5 becomes-2. They reject out-of-range/non-finite rounded values with
`invalid argument` before storing the integer result. The source tests execute
finite out-of-range conversions to both Int and Big; they do not claim new
non-finite/FP-environment coverage. Big-to-Int retains the low Word32 bits; the
suite includes `(1<<40)+7` producing7 and a negative value. Big-to-Real uses the
existing floating conversion, including its ordinary precision limits.

The compiler and loader derive source and destination kinds separately. A
conversion does not reinterpret an eight-byte Big as a Real. Arithmetic requires
Real operands, and every read must be definitely initialized on all incoming CFG
paths. Real/Big immediates, MP reads, prepared-frame operands, overlaps, hidden
middle arguments and writes into a differently typed span refuse before runtime
construction. No new wire opcode, signature kind or section is assigned.

## Reproduce the gates

```sh
python3 utils/tests/dis64-scalar-client.py --numeric --limbo /path/to/limbo
python3 utils/tests/dis64-scalar-client-boot.py --numeric \
  --limbo /path/to/limbo --root /path/to/root --emu /path/to/emu
python3 utils/tests/dis64-scalar-client-numeric-proof.py --limbo /path/to/limbo
python3 utils/tests/dis64-scalar-client-numeric-producer.py \
  --limbo /path/to/limbo --baseline /path/to/prior/limbo
```

Use the usual `CC`, `NATIVE64_ARCH` and `NATIVE64_RUNNER` for ARM64. The boot driver
has a separate `--runner`. The native-port gate integrates numeric execution,
raw proof and its three omission controls. The producer accepts an optional prior
compiler for byte compatibility; without it, it still checks deterministic output
and presence of every newly admitted operation. ASan/UBSan execution is scoped to
raw proof and constructor/retirement, not the whole running VM. Linux/QEMU results
do not establish an Android GUI APK or device startup.
