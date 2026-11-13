# Typed FP/MP source lowering for native arguments

The type-driven planner now supplies actual source origins and maps for the
existing private Sys variadic argument recipes. `native64callstores` uses
`dis64layoutstorage` and exact `dis64layoutaddress` reads for FP and MP. The old
algorithm that widened cells from descriptor bits is removed.
[Русская версия](dis64-typed-source-lowering.ru.md).

## Nested fields and a copied local

Save as `typedfield.m`:

```limbo
Probe: module {
    Inner: adt { count: int; text: string; wide: big; };
    Outer: adt { lead: byte; inner: Inner; fraction: real; };
    init: fn();
};
```

Save as `typedfield.b`:

```limbo
implement Probe;
include "sys.m";
include "typedfield.m";
original: Outer = Outer(byte 123, Inner(-17, "Grüße\0世界", big -17), -1.25);
init()
{
    sys := load Sys Sys->PATH;
    seed := original;
    sys->sprint("%d|%s|%bd|%g",
        seed.inner.count, seed.inner.text, seed.inner.wide, seed.fraction);
}
```

Inspect the ordinary compiler's private argument recipes:

```sh
/host/tools/limbo -I module -I . -Dq -o /tmp/typedfield.dis typedfield.b \
  2>/tmp/typedfield-calls.log
```

The output remains legacy Dis. The recipe is exercised by native tests below;
it does not admit this caller through `-64l` or make the old file executable in
an LP64 VM. No public wire profile is assigned.

| Field | Old relative offset | Native relative offset | Native bytes |
| --- | --- | --- | --- |
| `lead` | 0 | 0 | 1 |
| `inner.count` | 8 | 8 | 4 |
| `inner.text` | 12 | 16 | 8 |
| `inner.wide` | 16 | 24 | 8 |
| `fraction` | 24 | 32 | 8 |

The old extent is 32 bytes; native C needs 40. Word reads stay four bytes and
CVTWL sign-extends them into the existing eight-byte variadic slots. Big/real
preserve eight bytes. String moves use MOVP with retain/mark ownership.

## Producer and execution boundaries

Nested inline MP initializers resolve through real field declarations and AST
initializer nodes. UTF-8 lengths, embedded NUL, signed64 and canonical real bits
are preserved. References are not followed to invent constant heap contents.
Missing/dynamic inputs remain explicit.

A direct MP-to-FP MOVMP can define a field read by a later argument. The producer
checks copy Type, descriptor extent, exact field kind/width and coordinates,
plus existing branch/exception/call barriers. `cell.aggregate` records the real
copy opcode, descriptor and old source/destination bases. Several projected
fields may share that PC; the independent oracle requires the same original
instruction. Source policy `typed-word4-ref8` includes complete leaves and map8.
Padding, partial roots, cross-field scalar spans and borrowed-address argument
reads refuse. Failed bank construction releases its partial typed plan.

The recipe projects fields used by this argument path. Full managed lowering
must still preserve whole-copy aliases, ordering and all owners, including
unused fields. General copy chains, indirect heap reads, arithmetic inside a
prepared argument block and general control flow remain outside the producer.
Each original read gathers the current value, without caching the first read.

## Reproduce

```sh
python3 utils/tests/native64-callsites.py --limbo /host/tools/limbo
python3 utils/tests/dis64-layout.py --limbo /host/tools/limbo
python3 utils/tests/native64-arguments.py --formatter --limbo /host/tools/limbo
```

The last gate covers emu/os and WORD32/WORD32-file. Per combination it executes
768 existing real FRAME/MFRAME/MCALL/GC cases through four Sys gateways, plus
nested global/local/copied-field replay using actual VM move/conversion handlers.
A generated `.m` header supplies C sizeof/offsetof checks. Owning mapped storage
keeps MP inputs, FP values, gather cells and destinations live across collection
between steps; exact Unicode/NUL text, numeric bits and reference counts pass.

`--omission NAME` selects formatter omissions while retaining all positives and
ownership controls. Relevant choices: `typed-field-origin`, `source-map`,
`store-reference`, `fp-current-mp`. They must fail semantic predicates. Corrupt
copy origins/descriptors/projections also fail the classic Dis oracle. Compiler
ASan/UBSan, native-handler AMD64 UBSan and ARM64 UBSan/QEMU pass. Runtime-ASan
unwind evidence remains the open issue in [owned arguments](dis64-native64-arguments.md).

All 376 AWM storage plans still pass with unchanged ordinary bytecode. General
managed image emission/admission, ordinary variadic frame-tail execution, payload
descriptors, table writers and exceptions remain on the Android GUI path.
