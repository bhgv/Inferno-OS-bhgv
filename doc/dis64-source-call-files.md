# Limbo source to an executable Dis64 call file

[Русская версия](dis64-source-call-files.ru.md).

`limbo -64c` now emits complete scalar acyclic call programs for the existing
explicit `dis64-calls` file loader. The file is checked from its own bytes before
writing and again by the runtime. No compiler-local C declarations or native
pointer tables accompany execution.

## Compile and run

Save as `calls.b`:

```limbo
implement Calls;
Calls: module {
    init: fn();
    sum: fn(n: int): int;
};
init() {
    first := sum(7);
    second := sum(first);
}
sum(n: int): int {
    total := 0;
    while(n > 0) {
        total += n;
        n--;
    }
    return total;
}
```

With the native host compiler and an experimental WORD32/ref64 console emulator:

```sh
limbo -64c -o /path/to/inferno/programs/calls.dis64 calls.b
emu -v -r /path/to/inferno -c 1 -X dis64-calls -d /programs/calls.dis64
```

Use `-c 0` for C dispatch. `-64c` retains calls automatically by disabling source
inlining; adding `-i` produces identical bytes. The existing `-64`, `-64m`, `-64l`
and classic output paths keep their meanings. An ordinary/default launch refuses
this call file; the named experimental selection remains required.

The complete console logs normal root completion/retirement; this example prints
no application output. Its second sum is406, checked independently by the file
runtime gate. No Sys import is needed or admitted by this profile.

A wide-value example uses a real source conversion:

```limbo
implement Wide;
Wide: module {
    init: fn();
    twice: fn(n: big): big;
};
init() {
    n := -3;
    wide := big n;
    answer := twice(wide);
}
twice(n: big): big {
    return n+n;
}
```

`int` to `big` conversion has explicit raw inference/proof support in linear callers,
alongside the existing `int` to `real` rule. Source and destination kinds must
match, and a frame source must be definitely initialized. Call-containing helpers
also admit these two conversions. Call-free helpers retain their existing leaf
opcode rules; this does not open all numeric conversion combinations.

## Nested helpers

```limbo
implement Calls;
Calls: module {
    init: fn();
    mid: fn(n: int): int;
    leaf: fn(n: int): int;
};
init() { a := mid(7); b := mid(a); }
mid(n: int): int {
    a := leaf(n);
    b := leaf(a);
    return b;
}
leaf(n: int): int { return n+1; }
```

The final call returns11. `return leaf(n);` is supported too: the writer binds
its own outgoing tail and copies the value to its own incoming result pointer
after return. It never forwards that pointer to the next callee. This keeps
result ownership local at every depth. Shared callees form a DAG; neither self
recursion nor mutual recursion is admitted in this bounded file policy.

The verifier derives graph edges from actual raw CALL targets, proves callees
before callers and refuses a dependency cycle before publishing output. Linear
callers receive parameter initialization, exact FRAME/argument/result binding,
normal return coverage and budget checks. The earlier root/leaf checker is tried
first, preserving its accepted files and exact proof summaries. The independent
Python model instead uses DFS for cycles, its own linear value-state parser and
the previous CFG/return oracle for unchanged call-free leaves.

The raw checker covers94 positive/negative/budget/capacity cases, including both
cycle forms and late refusals, with unchanged records/count/image on failure.
Six active omissions detect lost self/mutual cycle protection, partial proof
publication, missing leaf return coverage and wrong binding/return owners. The
checker passes host ASan/UBSan and ARM64/Linux QEMU UBSan.

## Supported boundary

The root is an exported argument-free void `init`. It is linear and may contain
multiple calls and ordinary supported numeric operations. Helpers are exported scalar functions. Call-free leaf functions may contain checked
branches/loops, multiple returns and arithmetic return expressions. Helpers with
calls must be linear and end with RET; their dependency graph must be acyclic. Current scalar signatures use void/int/big/real;
void parameters are invalid. Recursive calls, branches in a call-containing body, module data/imports,
managed references, byte signatures, handlers and nonexported compiled helpers refuse the
complete source. Root must contain a call. Every compiled function must have a
matching declared export; no private helper is exposed or dropped implicitly.

The producer bounds total translated code to256 instructions, functions/types to64,
parameters to64 per function, frames to1MiB and image output to1MiB. Existing raw
proof imposes its further slot/initialization/resource rules. A valid Limbo source
outside those rules can still be refused by `-64c`.

These are the already used Dis64 v1.0/ABI1 Module, Code, Types, Exports and Strings
sections with version1 scalar signatures and existing ISA operands. No extension,
section, scalar, opcode or public profile ID is assigned. The CLI flag selects an
Inferno producer policy, not a new wire version.

## Translation and proof

The writer uses the existing typed multi-call lowering and native scalar layouts.
It orders init first, relocates every function/branch/call and emits exact scalar
signatures. Each frame gains two eight-byte cells: expression scratch and the
canonical outgoing result tail. Prepared calls bind that tail; a checked typed
copy moves the completed value into its source local. Arithmetic return stores
first compute into scratch and then issue an ordinary typed store through byte32.
Leaf branch destinations account for inserted instructions. Root RET becomes EXIT.

After constructing the whole container, the writer runs dis64callchaininfercheck over
those raw bytes. This independently derives numeric local spans and checks full
catalog/argument/initialization/call/result/return ownership before the first
Bwrite. Refused source or raw proof publishes no image payload. This is semantic
preflight, not a transactional filesystem rename guarantee for I/O failures.

The existing readmod/parsemod path repeats raw inference/proof and constructs its
own owned descriptors/code. It preserves startup-only selection, cache-profile
checks, ordinary admission callbacks and C/RX selection. The trusted whole-module
builder from [owned imports](dis64-owned-imports.md) retains its separate contract;
its C metadata is not used as file authority here.

## Reproduce

```sh
python3 utils/tests/dis64-source-calls.py --limbo /path/to/limbo
python3 utils/tests/dis64-source-calls-unit.py
python3 utils/tests/dis64-chain-check.py --limbo /path/to/limbo
python3 utils/tests/dis64-source-call-boot.py \
    --limbo /path/to/limbo --emu /path/to/emu --root /path/to/inferno
```

Use the existing CC/NATIVE64_ARCH/NATIVE64_RUNNER variables for ARM64/Linux QEMU
in the file runtime gate, and `--runner` for complete-emulator boot. All four
gates are included in native64-port.py. The writer IR unit supports host ASan:

```sh
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
python3 utils/tests/dis64-source-calls-unit.py
```

Twenty-eight source programs cover repeated calls, two arguments, loops, branch/return
relocation, entry reordered after helpers, int/big/real/void, negative values,
int32 min/max sign extension, nested fresh/forwarded results, multiple levels,
shared DAGs, intermediate conversions and zero divide at several depths. Independent Python parses
and checks raw wire/code/signatures against source reports and an independent call
oracle. Output is deterministic with/without explicit `-i`. Classic bytes match
the previous compiler when compiled with the same source path/options.

Each x64 and ARM64/Linux QEMU file gate runs all28 through real readmod/cache,
short reads, admission, scheduler, C/RX and retirement, failing every read/parse/
construct allocation in turn. Result bits are checked after execution, including
signed int32-to-big boundaries. Eighteen damaged files refuse, and ten unsupported
whole sources emit no payload. Existing inference9-image/30-refusal/312-runtime
checks and3343 catalog-bound cases pass. Active controls detect missing writer
proof, incorrect result binding, leaked relocation storage and wrong conversion
source/destination roles.

Rebuilt complete console emulators each run56 real OS roots from these files:
48 normal completions and eight zero-divide retirements, C/RX. Numerical result
checks are in the separate file gate; boot checks actual startup, worker and
retirement. Host compiler/writer-unit ASan/UBSan and runtime x64/ARM64 UBSan pass.
Full console boot is separately built with its normal target flags. Existing
runtime-ASan stack-unwind limitations and bounded fixture GC/OS seams remain.

## Android and managed modules

This is a working scalar source/file/console path on x64 and ARM64/Linux QEMU.
It does not serialize the general managed client/import graph, admit arbitrary
load dependencies or start AWM. Managed writer/loader, heap operations and Android
GUI remain ahead; no Bionic device run or ready APK is inferred.
