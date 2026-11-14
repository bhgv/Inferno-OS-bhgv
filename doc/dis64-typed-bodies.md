# Lowering complete typed function bodies

The new `limbo/dis64-typed.c` lowers a complete supported void function from
the compiler's checked IR into native FP/MP instruction operands. It consumes
the type-driven storage plans and retains the existing ISA opcodes. This is
the next consumer after [native argument recipes](dis64-typed-source-lowering.md).
[Русская версия](dis64-typed-bodies.ru.md).

## Example with AWM-shaped parameters

```limbo
implement Kernel;
include "sys.m";
include "draw.m";
Kernel: module {
    run: fn(ctxt: ref Draw->Context, argv: list of string, text: string, n: int);
};
answer: int;
result: string;

run(nil: ref Draw->Context, argv: list of string, text: string, n: int)
{
    total := 0;
    while(n > 0) {
        total += n;
        n--;
    }
    answer = total + len text + len argv;
    result = text;
}
```

Inspect the ordinary compiler's lowered body:

```sh
LIMBO_DIS64_TYPED=1 /host/tools/limbo -I module \
  -o /tmp/kernel.dis kernel.b 2>/tmp/kernel-typed.log
```

`DIS64_TYPED` records identify the function, original first PC, exact serialized
FP/MP descriptor ordinals, old/native extents, full map8, typed leaves, argument
offsets and lowered instructions `[opcode, mode, source, middle, destination]`.
Argument records retain kind and old/native widths, including channel/module
references, aggregate values and zero-size layouts; they are not inferred from
the first readable scalar field.
The example's parameters become:

| Argument | Native offset | Native bytes |
| --- | --- | --- |
| `ctxt` | 64 | 8 |
| `argv` | 72 | 8 |
| `text` | 80 | 8 |
| `n` | 88 | 4 |

The frame prefix/result-address convention is reserved independently. Root moves
use MOVP; numeric reads cannot consume a pointer's old four-byte cell. big/real
temporaries may share the compiler's unmarked eight-byte storage: the opcode
defines the active numeric interpretation. This never permits root/scalar reuse.

## Whole-body and branch checks

The lowering covers direct byte/Word/big/real/root moves, numeric conversions,
Word/big/real arithmetic and comparisons, String/list/array lengths, forward and
backward branches, and void returns. Exact per-opcode widths and reference kinds
are checked. Indirect/header operands, unsupported opcodes, value-return binding,
handlers and polymorphic/variadic function conventions refuse the complete body.
No partial code appears in a refused record. Owned temporary plans are released.

Each branch must point to a real PC within its own function. Global legacy PCs
become local instruction indexes without changing instruction order/count.
Middle-operand limits are retained. Eight-byte immediates refuse instead of
reading beyond a Word-sized immediate cell. Caller-supplied FP/MP must already
contain source-correct initialized data; this is trusted compiler IR lowering,
not a semantic validator for foreign bytecode.

## Execute the tests

```sh
python3 utils/tests/dis64-typed-body.py --limbo /host/tools/limbo --awm
python3 utils/tests/native64-arguments.py --formatter --limbo /host/tools/limbo \
  --omission typed-body-branch --omission typed-field-origin
```

The first gate independently decodes classic Dis and checks every direct
relocation, original opcode/mode, branch identity, descriptor extent/map and
unmodified legacy output. Corrupt target/root/offset/descriptor records refuse.
The C unit covers late refusal cleanup, reference classes, header boundaries,
function-relative PCs, handlers, wide immediates and code limits; three deliberate
guard omissions must fail their particular semantic assertions.

The second gate replays compiler-produced complete bodies with actual VM
move/arithmetic/conversion/length/branch handlers and GC between instructions.
Twenty cases per emu/os and WORD32/WORD32-file combination cover loops, early
returns, nil and Unicode/NUL Strings, nonempty list arguments, negative counts,
big/real conversions and result/global ownership. All source/global/gather data
stays in mapped owning storage; retirement balances heap/Type/reference owners.
Its private executor treats void RET as completion and restores its caller's
registers; it does not install a new production call-stack/scheduler path.

AMD64 UBSan and ARM64 UBSan/QEMU pass; compiler and unit ASan/UBSan pass.
Runtime-ASan unwind evidence remains the separate open issue documented in
[owned arguments](dis64-native64-arguments.md). Existing native argument and
768-case Sys call suites remain part of each combination.

## Actual AWM frontier

Across the sixteen startup sources, complete bodies of `wmsrv.hide`,
`wmsrv.unhide` and `sh.debug` lower; 357 other bodies refuse. Their legacy output
is unchanged. This does not execute those modules or AWM. The 376 existing FP/MP
storage plans continue to pass independently.

The ordinary output file is still legacy Dis. General Dis64 module writing,
initialized MP objects, imports/exports, calls and value-result binding, heap
payload operations, tables, exceptions and ordinary variadic startup remain.
No general module admission, new wire IDs, JIT-body claim or Android GUI APK
readiness follows from this private body replay.

Further native64 value-return work: [dis64-typed-returns.md](dis64-typed-returns.md).
