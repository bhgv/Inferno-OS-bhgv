# Source-derived imported calls on native64

[Русская версия](dis64-typed-external-calls.ru.md).

The [owned-module gate](dis64-typed-modules.md) used a C-written outer client.
This step lowers complete Limbo client functions containing imported calls and
executes their compiler-derived code against the owned source-derived provider.

## Limbo example

The interface remains an ordinary `.m` file:

```limbo
Provider: module {
    identity: fn(value: string): string;
};
```

A client may receive a module instance as an argument:

```limbo
implement Client;
include "provider.m";
Client: module {
    keep: fn(p: Provider, value: string): string;
    forward: fn(p: Provider, value: string): string;
};
keep(p: Provider, value: string): string {
    first := p->identity(value);
    second := p->identity(nil);
    return first;
}
forward(p: Provider, value: string): string {
    first := p->identity(value);
    return p->identity(first);
}
```

The first form retains a local result across another call. The second can pass
its caller-owned result address through the final call. A library needs no entry
function; nothing automatically invokes `init`.

Inspect the private report with:

```sh
LIMBO_DIS64_TYPED=1 LIMBO_DIS64_TYPED_EXTERNAL=1 \
    /path/to/limbo -i -I /path/to/interfaces -o Client.dis Client.b
```

`-i` retains source calls by disabling inlining. The output file is still classic
Dis, byte-identical to the same compilation without reporting. The report is
trusted compiler metadata, not a new executable file format.

## Existing import identities

The final compiler IR already carries each imported function declaration.
`iface->offset` supplies its import ordinal; existing `sign(Decl*)` supplies its
32-bit hash. Export names retain ADT qualification. The new lowerer consumes
those identities rather than assigning alternative signatures or public IDs.

A canonical call consists of MFRAME, argument moves, an optional result binding
and matching MCALL. Both endpoints must identify the same module operand and
import declaration. Module operands must be typed module roots in FP or MP.
Import ordinals fit the existing signed middle-immediate range, 0 through32767.
Arguments preserve declared order, source type, old coordinates and native
alignment. Scalar arguments are byte/int/fix/big/real; managed references are
full-width roots. Aggregate copies, variadics, polymorphic calls and handlers
remain refused. Arithmetic arguments must first be computed into ordinary locals.

Branches may reenter MFRAME, but cannot enter its preparation interior. An
inherited result becomes defined only after normal MCALL completion; argument
moves and local address preparation do not establish a return value. Both local
and imported call sites use the shared bounded function/CFG analysis.

The private `inferno-typed-external-v1` report adds `externals` objects containing
relative site endpoints, import ordinal, module space/offset, canonical name/hash,
argument geometry, result destination/contract and inheritance. Code rows retain
the six-field representation. Existing local/multi/body reports remain unchanged
when the new reporting flag is absent.

## Native materialization and ownership

`Native64TypedCall` extends its private checked-IR description with an external
flag and module space/offset. Local calls leave the added fields zero. Imported
sites use the linked export Type for frame bounds and an import ordinal; they
retain no native target PC. Production MFRAME and MCALL select the linked target
at execution. Both endpoints are checked against metadata before any output is
written, as are setup isolation, argument spans and result contracts.

The runtime gate assembles the client Module/outer harness in trusted C and
materializes its complete source bodies; it uses the existing owned constructor
for the provider. Exact name/hash linking, MP cloning, frame growth, return and
error teardown use production code. This is not automatic import-table assembly
inside Native64ModuleSpec, module-cache admission or a general image loader.

## Run and evidence

```sh
python3 utils/tests/native64-typed-external.py --limbo /path/to/limbo
python3 utils/tests/dis64-typed-external-unit.py
```

ARM64/Linux QEMU uses the existing CC/NATIVE64_ARCH/NATIVE64_RUNNER settings.
Both gates are part of native64-port.py. For host constructor/materializer ASan:

```sh
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
python3 utils/tests/native64-typed-external.py --limbo /path/to/limbo --materialize-only
```

The source oracle checks17 complete functions/33 imported sites against classic
Dis and serialized `.m` imports, plus mixed byte/int/big/string argument geometry.
Each x64 and ARM64/Linux QEMU execution passes912 cases:768 ordinary pairs,
48 source loop-reentry cases and96 managed continuation faults. All four
caller/provider C/RX choices, stack growth, quanta1/3/1000, scalar widths, nil
reference arguments, fresh/inherited results and separate MP clones are covered.
String/list values remain opaque owned graphs in this fixture. Source multi-argument
geometry and MP module operands have compiler/materializer evidence respectively;
they are not additional multi-argument or MP-source runtime cases.

Eight malformed materializer inputs leave every output byte untouched. Three
active materializer controls remove module alignment, import bounds or setup
isolation; a fourth runtime control removes exact signature matching. Four
compiler controls remove module identity, argument coordinates, import bounds
or inherited-result tracking. Precise semantic failures are required, not crashes
or sanitizer diagnostics. Compiler IR and constructor/materializer ASan/UBSan
pass; runtime uses UBSan. The existing runtime-ASan unwind limitation remains.

## Remaining Android boundary

A fixture-only `loader` function emits real serialized import hashes and is
explicitly refused by the typed-body lowerer. No runtime `load` execution is
claimed here. Both existing managed file profiles (`-64m`, `-64l`) still refuse
this source client. Namespace/cache admission, general managed writer/loader,
remaining heap operations and Android GUI startup remain unfinished. Existing
scalar/Sys/file profiles retain their own permissions and evidence.

All16 AWM source outputs/376 layouts retain classic bytes; the default body gate
remains14 accepted/346 refused. No Android/Bionic device, AWM execution or ready
APK is inferred from Linux QEMU.
