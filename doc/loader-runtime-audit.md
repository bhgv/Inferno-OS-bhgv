# Dynamic Loader and descriptor audit

## Closed defects

- `dtype` rejected neither negative sizes nor null/truncated/oversized pointer
  maps. It now bounds map storage, requires each marked native pointer slot to
  fit the object, requires pointer-bearing element sizes to preserve pointer
  alignment across arrays, and explicitly initializes helper pointers with `mallocz`.
- `Loader_tnew` leaked a newly constructed Type if its table could not grow.
  It now releases that Type and checks table-size arithmetic before growth.
- `Loader_dnew` retained the creator's Type reference after `heapz`, skipped nil
  maps, leaked the Type on a VM heap error and overwrote existing results
  without releasing them. Nil maps now mean no pointers; the runtime protection
  boundary catches allocation errors, balances references and releases old
  return values. Input WORD sizes/map lengths are checked before conversion to
  the runtime's int fields, including on the host LP64 model.
- `Loader_compile` lost the original MP and left the temporary MP installed
  after a non-local compiler error. It now restores the saved value on all
  protected returns; disabled compilation leaves it alone.
- `Loader_ext` interpreted a Dis PC index as an Inst offset into native code
  after compilation. Export replacement is now rejected for such modules.

- `Loader_newmod` could dereference a failed name allocation and lose partial
  module state on a string/heap error. It now retains ownership outside the
  runtime error frame, cleans up every failed stage, explicitly initializes
  metadata, and publishes only after mklinkmod succeeds. It creates an empty
  export sentinel table required by unload and native-PC JIT metadata walks.
- Dynamic instruction count/element sizes, link allocation sizes and opcode,
  address and middle-operand fields are checked before narrowing or copying.
  Branch relocation is restricted to immediate destinations, matching parsemod.

## Verification

`libinterp/tests/loader-runtime.py` extracts the production functions and uses
real generated Loader frames, VM types and emulator Proc error frames. Heap and
compiler operations are injected boundaries. It checks each bitmap boundary,
allocation/resize failures, old-result replacement, exact Type reference
counts, 100 repeated heap/compiler errors, nested cleanup and MP restoration.
The same harness checks newmod allocation cutoffs, string conversion and
mklinkmod errors, data/Type reference balance, old-result replacement, empty
exports, wide lengths, invalid instructions and conditional branch relocation.
Explicit allocation accounting ends at zero. UBSan and ASan host checks pass;
ASan stack instrumentation is disabled for the longjmp harness because this
sandbox does not supply its expected /proc stack information.

```sh
CFLAGS='-O2 -g -fsanitize=undefined' python3 libinterp/tests/loader-runtime.py
CFLAGS='-O1 -g -fsanitize=address,undefined --param=asan-stack=0' \
  python3 libinterp/tests/loader-runtime.py
python3 libinterp/tests/riscv-runtime.py
```

The active bitmap still advances by native pointer-width slots, matching the
current heap scanners. This change does not activate the separate Dis64 ABI
proposal's four-byte bitmap quantum. Real 32-bit builds, full GC, concurrent
module/link ownership and native compiled calls remain integration checks.

## Snapshot ownership and exported metadata

`Loader_ifetch`, `Loader_link` and `Loader_tdesc` now use checked heaparray
allocation and initialized pointer maps. A protected construction context owns
partial results until completion; failed child allocations release the array,
previously constructed children and all descriptor references. Returned old
arrays are still released on replacement. Descriptor maps with zero bytes
remain H and exports retain their existing reverse order.

Branch recovery now subtracts the native Inst pointers before storing a Dis
index in Loader_Inst.dst. The previous path first copied the pointer into WORD,
which loses its high bits when WORD is narrower than a native pointer.
Loader_link rejects native-PC compiled modules, like Loader_ifetch, because
its integer pc field represents a Dis index. Leaf JIT modules keep Dis PCs and
remain inspectable. Builtin exports retain pc/tdesc = -1. Frame lookup is bounded
by ntype and returns an error for an absent frame instead of overreading the
type table.

`libinterp/tests/loader-snapshots.py` runs 231 cases per native/word32 model with
production heap initialization and pointer cleanup. It independently reproduces
the old ownership, branch, metadata and size defects. Both strict UBSan and ASan
with stack instrumentation pass; see tests/README.loader-snapshots.md for scope.

## Module-instance data and failed initialization

`arraycpy` now follows pointer maps in ordinary element descriptors, cloning
array-valued fields through newmp. The old identity comparison with the global
Tarray heap-object descriptor did not recognize user array element descriptors;
nested initial arrays were therefore shared between module instances. Scalars
still use a bulk-copy path when the element map has no pointers. Non-array
references retain their previous shared-reference semantics.

A protected array-copy context owns each partial result. heaparray initializes
all mapped slots to H before recursion. Failure destroys completed children
and propagates the original VM error. newmp clears destination pointer slots
before acquiring owners, then publishes only a completed child clone or an
incremented non-array reference. Its map traversal uses byte offsets of native
pointer slots, including transitions between bitmap bytes. A failed destination
can be destroyed without releasing unacquired references belonging to the
source template.

`linkmod` now owns its partial Modlink outside the protected initialization
callback. An ordinary binding failure cleans up and returns H. An allocation or
initializer VM error cleans up and is rethrown. Even a failed first allocation
releases the module reference passed to linkmod. Instance tests include an
injected dynamic-data hook boundary; they do not execute real native libraries.

`libinterp/tests/heap-clones.py` checks 189 cases per native/word32 scalar model,
including every allocation failure in recursive trees, mixed records, partial
MPs and module instances. Original bodies independently fail all three defect
families. Strict UBSan and ASan with stack instrumentation pass. See
`libinterp/tests/README.heap-clones.md` for the boundary injections and remaining
GC, cyclic-graph and full-ABI integration limits. Scheduler startup is covered
separately by stack-startup.py and doc/stack-startup-audit.md.

## Growing type tables and checked VM consumers

Loader_tnew updates the owner table and its supplied instance, but another
Modlink can retain a pointer to the freed pre-realloc table. All interpreter
type consumers now resolve descriptors through Module.type. A shared helper
checks the full WORD index against ntype, the table/descriptor presence and
nonnegative descriptor size before any instruction effects. This covers object,
array, mapped channel/list/record and frame operations. Modlink.type remains
only as a legacy snapshot to preserve structure layout; future JIT ports must
use the owner table or stable Type addresses rather than this cache.

mcall now uses the same checked import/export resolver as mframe and mspawn.
The complete WORD index is resolved before writing the caller frame, changing
registers or acquiring the callee reference. This removes int truncation,
negative-index arithmetic overflow and unchecked linkage-table accesses while
preserving normal Dis calls and builtin caller restoration.

module-types.py runs 844 cases across native and word32 scalar models. Both
strict UBSan and ASan pass; previous opcode bodies independently reproduce the
stale-table use-after-free and the unchecked-call overread. See
libinterp/tests/README.module-types.md for boundaries and remaining integration
work. Native-PC compiler verification and the requested new JIT ports are not
claimed complete by these interpreter fixes.
