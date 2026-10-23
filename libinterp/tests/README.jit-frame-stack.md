# JIT frame stack boundaries

The C `rmfram` helpers in ARM, 386, MIPS, Power and SPARC now validate the
source descriptor and destination, reject frames smaller than `Frame`, and
compare the requested size with the remaining stack bytes before forming a
new pointer. Missing or exhausted stack spans go through `extend`; failed
growth must leave the destination and current stack bounds unchanged.

The native `macfram` paths reject unsigned address wrap before comparing the
new stack pointer with the extent limit. 386 and SPARC also use unsigned
comparisons, so crossing the signed-address boundary does not change the
allocation decision. Equality with the limit retains the existing slow path.
The native paths assume a valid current VM stack and descriptor; this does
not replace complete validation of all module metadata.

Run from the repository root:

```sh
python3 libinterp/tests/jit-frame-stack.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/jit-frame-stack.py
python3 libinterp/tests/jit-frame-addresses.py
```

`jit-frame-stack.py` executes the five production C helpers with actual VM
headers: 120 cases per backend (600 total), covering nil/H operands, negative
and undersized descriptors, `INT_MAX`, missing/reversed/exhausted stack
bounds, successful inline allocation and successful/failed extension.
Scheduler and allocator boundaries are stubs.

`jit-frame-addresses.py` requires Python `unicorn` and a host C compiler.
It compiles production `macfram` functions and instruction encoding macros,
then executes their address arithmetic and branch decisions in five CPU
models: 240 cases per backend (1200 total). It covers equality, the signed
address boundary and 32-bit wrap. Descriptor offsets, constant/call emission
and selected memory emission are boundary stubs. Execution stops on the
first stack-pointer publication or slow-path type store, before initialization
or external calls. It does not validate a complete native calling convention,
physical CPU pipeline or complete Inferno execution.

Set `JIT_STACK_ARCH` to one of `386`, `arm`, `mips`, `power`, `sparc` and
`JIT_STACK_SOURCE_DIR` to an alternate directory of backend sources to check
an earlier revision. Both drivers reject the pre-fix sources for every backend.
These checks concern existing 32-bit backends; x86-64, AArch64 and RV64 still
need separate layout and ABI work.
