# Owned native64 clients and import tables

[Русская версия](dis64-owned-imports.ru.md).

The [external-call gate](dis64-typed-external-calls.md) materialized client bodies
inside a hand-assembled client Module. The whole-module constructor now builds
that client, including owned import tables, function code/maps/exports, local
relocations and imported call sites. The provider can be constructed independently.

## Interface and source

A normal interface still describes the callable functions:

```limbo
Provider: module {
    count: fn(n: int): int;
};
```

A client library may contain both imported and local calls:

```limbo
implement Client;
include "provider.m";
Client: module {
    fresh: fn(p: Provider, n: int): int;
    local: fn(p: Provider, n: int): int;
};
fresh(p: Provider, n: int): int {
    first := p->count(n);
    second := p->count(first);
    return second;
}
local(p: Provider, n: int): int {
    return fresh(p, n);
}
```

Both functions belong to the complete accepted library; it needs no command entry
and does not automatically invoke init. Source reports and interface probes supply
the private C tables. Ordinary compiler output remains classic Dis.

## Constructor contract

Native64ModuleSpec adds `tables`/`ntables`. Each Native64ModuleTable contains a
counted array of Native64ModuleImport records: name, existing32-bit interface hash,
native argument-frame bound and its pointer map. These argument descriptors cover
the ABI parameter area; they are independent of a provider's additional locals.
The constructor therefore needs no loaded provider or native target PC.

The constructor owns a sentinel-terminated Module.ldt table-of-tables, each
sentinel-terminated Import array, copied names/signatures and Type/map descriptors.
Import argument descriptors follow function descriptors in Module.type. Data
remains ordinal0; function ordinals remain index+1. No Module/Frame layout changes
or new public Dis64/ISA/Sys IDs are introduced.

Native64ModuleSite retains its local fields and adds external/table/module-space/
module-offset fields. Local sites leave the added fields zero. External `target`
is the requested import ordinal inside `table`; local `target` remains a function
index. All sites share the existing atomic typed-code materializer. Module operands
must point to complete mapped FP/MP root cells. The compiler remains responsible
for their module type and semantic ABI provenance.

Limits are64 nonempty tables and4096 total requests, in addition to existing code,
function and32MiB aggregate geometry bounds. Import frame bounds are64..65536 bytes,
aligned to8, with exact maps and no roots in the reserved frame header. Preflight
checks table/ordinal references, module cell geometry and root roles before any
allocation. Partial tables/names/maps/programs/exports/MP/JIT remain owned until
successful publication or full rollback. An unregistered module's original MP
keeps the existing owner pin.

## Link using the existing runtime

In the usual protected VM context, after constructing trusted client/provider
owners:

```c
provider.module->ref++; /* linkmod consumes this extra reference. */
ml = linkmod(provider.module, client.module->ldt[0], 1);
if(ml == H)
    return 0; /* Missing export or incompatible existing signature. */
/* Place ml in a GC-visible VM root before further collecting work. */
```

linkmod resolves the whole requested table with production name/hash checks and
clones provider MP. It retains partial instances and temporary roots through
fallible work, consumes its input Module reference on every path, and rethrows
allocation errors through normal VM error handling. A late binding failure retires
all earlier bindings/clone ownership. Runtime MFRAME/MCALL uses the linked actual
export frame and target, not the import-bound Type used during construction.

Keep both owner guards alive while their modules/borrowed import metadata are in
use. They cannot be copied, and clear refuses while instances/stacks retain a
module. This is a serialized trusted-C lifetime contract; it adds no concurrent
revocation, lifetime epochs or namespace/cache admission. Matching a classic hash
alone does not authorize mixing a legacy32-bit module with a native64 instance.

## Run and evidence

```sh
python3 utils/tests/native64-typed-imports.py --limbo /path/to/limbo
```

The gate is included in native64-port.py and accepts the existing ARM64/Linux
CC/NATIVE64_ARCH/NATIVE64_RUNNER settings. Constructor/link-only host sanitizers:

```sh
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
python3 utils/tests/native64-typed-imports.py --limbo /path/to/limbo --materialize-only
```

All18 functions/exports of a loader-free client are constructed:33 imported sites
and one local site. Separate classic Dis/.m probes supply23 import identities and
argument shapes, including seven unused leaf declarations retained by the module
interface. The constructor does not need provider bodies to infer those shapes.
The imported/local client bodies and exported hashes are checked independently
against actual classic instructions and exports.

Each x64 and ARM64/Linux QEMU run passes960 three-module cases:768 ordinary,
48 source-loop,48 local-wrapper and96 managed continuation faults. The C outer
harness calls an exported client function; both complete client and provider are
owned constructor outputs. Production linkmod/MFRAME/MCALL/RET and clone/destruction
run through all four client/provider C/RX choices, growth, quanta1/3/1000, inputs,
scalar widths, fresh/inherited results, nil roots and complete allocation/pin drain.
String/list identity values remain opaque owned graphs in this fixture.

Six constructor matrices fail every allocation position:93/94 for one import
table without MP,140/141 for two tables without MP and142/143 for two tables plus
mapped/pinned MP, respectively C/JIT. Ten malformed inputs refuse before allocation.
Second-table relocation and copied name/hash/map independence pass. A mapped-MP
module operand has construction evidence; source execution still uses FP modules.
Actual linkmod rejects missing/late wrong hashes and cleans up all four clone
allocation failures. Three active controls remove table bounds, mapped-root roles
or exact link hashes; only their precise semantic failure counts.

Host constructor/link ASan/UBSan and x64/ARM64 runtime UBSan pass. Runtime-ASan's
existing stack-unwind limitation remains open. GC pin, OS and collector seams are
bounded fixture support, not a concurrent collector proof. Prior host whole-module
960 and external-source912 gates and their active controls remain green.

## Remaining boundary

This advances trusted native client assembly and ordinary import linking. General
managed image writing/reading, source load admission, namespace/cache provenance,
remaining heap operations and Android GUI startup remain separate work. Dynamic C
module initialization is outside this fixture. Existing scalar/Sys/file profiles
keep their own supported paths. No new AWM execution, Android/Bionic device run or
ready APK is claimed by Linux QEMU results.
