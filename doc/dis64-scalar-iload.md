# Structural scalar imports through ILOAD

[Русская версия](dis64-scalar-iload.ru.md).

The production ILOAD handler now has an explicitly installed structural-import
route to the [namespace library reader](dis64-scalar-namespace.md). This joins the
actual VM instruction, namespace admission and whole-provider proof. The caller
in the current execution gate is still constructed from trusted native metadata;
a serialized importing Limbo program is not yet admitted by this change.

## Install the interface before publishing a caller

Generate each interface from its ordinary `.m` file:

```sh
limbo -H -R -M Library library.m > library-native64.h
limbo -64b -o library.dis64 library.b
```

A trusted checked-native caller constructor can attach the complete interfaces:

```c
#include "library-native64.h"
Native64ImportRequest tables[] = {
    {limbo_native64_Library_scalar_imports,
     limbo_native64_Library_scalar_strings,
     sizeof(limbo_native64_Library_scalar_imports),
     sizeof(limbo_native64_Library_scalar_strings)}
};
if(!install64callimports(caller, tables, nelem(tables), 1))
    return 0; /* Retire the unpublished caller through its normal owner. */
```

`caller` must be an unpublished, unregistered, exclusively owned native typed
Module, with `native64typedlea8step` selected by its checked constructor. Existing
classic LDTs or an already installed structural table cause refusal. Installation
requires the explicitly selected `dis64-calls` startup policy. No image flag,
filename or signature chooses this host policy. Passing JIT 0/1 sets the request
for subsequently loaded providers; the reader still checks current `dontcompile`
after I/O.

One native allocation owns all table descriptors and their Imports/Strings bytes.
All tables are validated before allocating or publishing anything. Allocation
failure leaves both private Module fields empty. Limits are64 tables,64 scalar
functions/arguments per table,1MiB per payload and8MiB for the complete allocation.
Zero tables refuse; a table containing zero function requests is valid. Inputs
remain immutable and readable during installation, then may be discarded.

This trusted API validates request metadata, not caller code, root maps or
instruction operands. Those remain the checked native constructor's contract.
Installing these requests is not admission of arbitrary CODE or managed file
bytes. The opaque owner and its trusted reader callback occupy two new private
Module fields. Rebuild all runtime objects after this native layout change.
Frame64, Modlink, the Dis64 wire layout, public IDs and signatures are unchanged.

## Execute load and call

For this caller, ILOAD's middle operand is a structural **table index**. MFRAME
and MCALL select a function within the returned instance using the generated
`LIMBO_NATIVE64_IMPORT_Library_<function>` ordinal. A table index and a function
ordinal are different coordinates. No legacy32-bit hash substitutes for the
structural signature.

The source operand is an ordinary VM String path; the destination must be a
properly mapped module root established by the checked caller. The installed
reader validates the signed table index, then performs a fresh uncached namespace
read with release/isave and acquire/irestore, file policy and complete provider
proof. Out-of-range/negative indices raise `exModule` before namespace I/O.

A successful result retains its external GC pin until the ILOAD handler marks and
publishes it in the destination. Then ILOAD removes the temporary pin and destroys
the previous destination owner. Repeated loads therefore replace and retire the
old instance. Ordinary load refusal publishes H and retires the old destination;
it does not automatically execute init. An invalid table index raises before
changing that destination. `$Sys` and `$self` remain outside this structural
library route. Modules without an installed callback retain their classic and
existing trusted builtin paths.

The importing Module owns its request block until normal `freemod` teardown.
It stays live through the current VM context across the released I/O interval;
no extra globally registered table or borrowed compiler memory survives.

## Reproduce

```sh
python3 utils/tests/dis64-library-namespace.py --limbo /host/limbo --iload
for control in copy pin route; do
    python3 utils/tests/dis64-library-namespace.py \
        --limbo /host/limbo --iload --iload-control "$control"
done
ASAN_OPTIONS=detect_leaks=0 \
NATIVE64_CFLAGS='-O1 -g -fno-strict-aliasing -fsanitize=address,undefined -fno-sanitize-recover=all' \
    python3 utils/tests/dis64-library-namespace.py \
        --limbo /host/limbo --iload --materialize-only
```

The existing CC/NATIVE64_ARCH/NATIVE64_RUNNER variables select ARM64/Linux QEMU.
The instruction matrix loads twice through different tables (empty requests, then
the complete `.m` interface), checks each instance link count, then executes
the actual provider with MFRAME/MCALL. Four caller/provider C/RX choices, fitting
and growing stacks and quanta1/3/1000 exercise ordinary completion, provider
faults, open/admission refusal and both invalid index directions. Input interface
bytes are poisoned before execution. At instruction boundaries, temporary pins
must be gone; full process retirement drains both instances, filename String,
request owner, Types, JIT and namespace descriptors. Installation also checks
all64 tables, empty requests, late invalid metadata and allocation failure.

The14 compiler interfaces/providers execute1680 roots per target. Two independent
`.m` mutations (unused missing export and Big/Real mismatch) add240 refusal roots.
The copy/pin/route controls remove one active property and require a semantic
exit10, without sanitizer/undefined-behavior failures. ASan/UBSan covers installation
and retirement without VM instruction execution; the earlier runtime-ASan unwind
limitation remains open.

Namespace transport/admission hooks, ASCII filename conversion and pool/collector
support are bounded fixtures. Real reader, ILOAD, scheduler, MFRAME/MCALL, return,
reference destruction and accepted native leaf entries execute. These tests do not
prove concurrent filesystem/collector behavior, a newly admitted importing Limbo
file, Android/Bionic, AWM or a working GUI APK.
