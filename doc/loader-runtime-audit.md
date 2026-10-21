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
