# Source-derived bodies in actual native64 calls

[Русская версия](dis64-typed-calls.ru.md).

The [previous value-return gate](dis64-typed-returns.md) ran real opcode handlers
against staged heap buffers. `native64-typed-code.h` now materializes those same
checked compiler bodies into native `Inst` arrays. The new gate places them in
an actual Module and executes production FRAME, CALL, RET, decoder, stack growth,
error protection and the owning architecture's published RX entry.

## Reproduce

From the repository root, use a host Limbo built from this branch:

```sh
python3 utils/tests/native64-typed-calls.py --limbo /path/to/host/limbo
```

ARM64/Linux cross execution uses the existing port-gate environment:

```sh
CC=aarch64-linux-gnu-gcc NATIVE64_ARCH=arm64 \
NATIVE64_RUNNER='qemu-aarch64 -L /usr/aarch64-linux-gnu' \
python3 utils/tests/native64-typed-calls.py --limbo /path/to/host/limbo
```

`native64-port.py` now runs this gate automatically with its host tools and target
runner. The complete compiler inputs are in `dis64-typed-body.py`: triangular
int count with a loop, byte, big, real, string identity and list identity. Both
native offsets and maps come from the compiler; the independent classic Dis
oracle still verifies every relocation before C code is generated.

## Materialization contract

The shared `Native64TypedStep` preserves the six-field v2 private report contract.
The adapter requires Word32/ref64 geometry, bounded aligned FP/MP extents, a
sufficient disjoint output array, supported opcodes and representable operands.
Branches become native instruction pointers inside the newly materialized body;
a body may be embedded at a nonzero offset in a larger module. A result store
becomes native indirect FP[32]+0, resolved when the callee runs. It captures no
caller or stack address. No allocations or callbacks occur in the adapter, and
all structural checks finish before any output bytes change.

This consumes immutable trusted checked IR. Exact semantic kinds, operand widths,
GC maps, initialized reads and valid result contracts remain compiler premises;
this adapter is not a validator for arbitrary supplied code or metadata. It does
not enable FRAME/CALL in the source-body lowerer or the file loader. Input and
output storage must remain disjoint. No public format or opcode IDs are assigned.

## Real calling sequence

The fixture's caller is constructed by trusted C, following the existing private
native call gate. It allocates a frame with FRAME, copies the argument using its
actual width and copies eight native address bytes into callee offset32 with
MOVL. Ordinary Word32 LEA cannot establish that pointer. CALL enters either the
source-derived leaf or one intermediary wrapper. The wrapper prepares another
frame, forwards the argument and original return address, calls the leaf and
returns normally. RET releases mapped callee references and restores the caller.

The prepared caller pointer lives in header cell40; trusted address scratch is48;
argument64 and result72 are separate fields. Neighboring bytes are canaries.
These live stack frames are rooted by the production stack mechanism, not the
heap-buffer Native64TypedReturn helper, which retains its separate trusted scope.

## Evidence and limits

On each of x64 and ARM64/QEMU, 288 executions cover six result kinds, C/RX,
root-only or growing extents, direct/nested calls, quanta 1/3/1000 and two input
variants. Scalar extrema and byte/int neighboring bytes survive. String/list
identity bodies transport mapped reference owners: these call-specific fixtures
use opaque two-level owned heap graphs, not String decoding or list operations.
They prove reference replacement/alias/nil and exact ownership after both callee
returns and full teardown. The previous real String/list/Unicode/GC handler gate
continues separately. This bounded call fixture supplies allocation/OS/collector
boundaries; it does not run a concurrent collector or OS scheduler.

Thirteen structural refusals preserve all output bytes. Two active controls remove
the local branch upper bound or signed16 middle bound: each must exit10 at its
specific semantic refusal assertion, never by timeout, crash or sanitizer error.
The C/RX runtime matrix uses UBSan and explicit heap/extent/ownership accounting.

General source call lowering, module CALL/RET admission and the managed module
writer/loader remain unfinished. AWM whole-body analysis stays14/346. This is
not an Android/Bionic device result, an AWM run or a ready GUI APK.
