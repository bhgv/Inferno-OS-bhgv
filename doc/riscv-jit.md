# RISC-V JIT: experimental leaf backend

The `experiment` branch contains a real RV32I/RV64I encoder in
`libinterp/riscv-emit.h`, integrated through `comp-riscv.c` and
`comp-riscv64.c`. It replaces the disabled x86 code previously stored under
the RV64 name. This is a first-stage backend, not a complete or benchmarked
RISC-V OS port.

## Execution model

Like the existing backends, it uses the Dis decoder and `optab` helpers.
Unlike the older native-PC backends, it retains original `Inst` addresses,
link PCs, return PCs, exception metadata and case tables. A per-module `Jit`
contains native leaf functions indexed by opcode. `xec` decodes operands,
advances the Dis PC, invokes the native leaf if present, and uses `optab` for
missing leaves or a leaf returning 0. Every instruction consumes the same
scheduler quantum as before.

The existing `compiled` flag continues to mean relocated native PCs, and
remains 0 for leaf-JIT modules. `Loader_compile` now copies `m->compiled`
instead of unconditionally setting the Modlink flag to 1. Other backends
still set `m->compiled=1` themselves. Existing module calls, exceptions,
reference counting, allocations, GC and system calls continue through their
existing interpreter implementations. `typecom` does not emit type helpers.

Code is generated once for each supported opcode actually present in a
module. Looking up the **current opcode**, rather than an old per-PC copy,
keeps runtime instruction replacement safe. Newly introduced opcodes without
a generated leaf use `optab`.

The native signature is `int leaf(REG*)`: a0 holds REG on entry; a0=1 means
completed, a0=0 requests helper fallback without changing VM memory. Only
caller-saved t0-t6 are clobbered. Leaves do not touch sp or callee-saved
registers and do not call C. Operand slots are loaded/stored with 32-bit
halves when needed, so 8-byte values at 4-byte-aligned Dis offsets do not
require unaligned LD/SD. REG pointer fields use the native pointer width.

Native operations include byte/word moves, bit-preserving real/big moves,
byte/short/word conversions, compatible-width lea, add/subtract, bitwise
operations, shifts, M-extension multiplication, jumps and six byte/word
relations. Out-of-range shift counts return 0 before any visible store.
With the division extension, bounded RV64 integer division/modulo have native
paths; zero divisors decline to the C handlers to preserve VM exceptions. Without
the extension they remain in C. Float arithmetic also stays in helpers: the K210 target is RV64IMAFC, without D.

The encoder supports RV32/word32, RV64/word32 and the repository's current
RV64/legacy-word64 configuration. The last matches the current interpreter's
C types; it **does not repair the incompatible LP64 layout of ordinary Dis
files**. Passing leaf tests is not evidence that the entire 64-bit VM works.
RV64/word32 is the intended arithmetic profile for a future proper 64-bit
pointer ABI, not an enabled replacement for all existing VM structures.

## Publication and lifetime

Generation has a bounded sizing pass and an emission pass. No module state
changes until allocations and `segflush` succeed; failures free temporary
allocations. A runtime protection boundary also releases unpublished code
after VM error() unwinds, including a cache-sync exception. Null program
buffers are rejected before opcode scanning. Module destruction frees both native code and the Jit table.
The new Module field is initialized at all three Module allocation sites.

FreeRTOS RV64 `segflush` now issues `fence.i` instead of doing nothing.
Because `fence.i` is local to a hart, the backend also runs a static native
entry barrier on the executing hart before invoking generated code. This
conservative per-leaf barrier handles task migration; a later per-hart epoch
can reduce the cost. It is not intended as the final performance design.
The port still relies on its existing allocator providing executable memory.
A future hosted Linux RISC-V port needs an executable allocator and the Linux
RISC-V cache synchronization API; no Linux RISC-V port is added here.

The existing FreeRTOS build target now lists `CPUS=riscv64`, and its empty
architecture mkfile explicitly selects no ARM assembly. SDK/toolchain setup
and the remaining FreeRTOS port dependencies must still be provided.
RV32 source support does not by itself add a turnkey RV32 host/OS target.

## Reproducible checks

```sh
python3 -m pip install unicorn==2.1.4 capstone==5.0.9
python3 libinterp/tests/riscv-jit.py
python3 libinterp/tests/riscv-runtime.py
CFLAGS='-O1 -g -fsanitize=undefined' python3 libinterp/tests/riscv-jit.py
CFLAGS='-O1 -g -fsanitize=undefined' python3 libinterp/tests/riscv-runtime.py
```

`riscv-jit.py` compiles the production encoder with a host C compiler and
executes its RV instructions in Unicorn. It checks 1910 cases across three
profiles: arithmetic order, overflow/truncation, signed/unsigned relations,
shift fallback, addresses and PCs above 4 GiB, overlapping moves, slot
boundaries, buffer limits, M-extension selection and ABI preservation.
Capstone validates instruction decoding, including RV64 ADDW selection.

`riscv-runtime.py` extracts the production compile/Loader_compile/freemod/xec
paths and runs them on the host. Native execution is replaced with C stand-in
callbacks in this test. It checks allocation/cache/VM-error failures, atomic
publication, repeated compilation, destruction, opcode changes, fallback,
module switching, PC advancement, quantum accounting and native-PC dispatch.
These two test boundaries are complementary; neither boots Inferno or
validates the complete FreeRTOS SDK on hardware.

## Specification references

- RV64 word instruction semantics:
  <https://docs.riscv.org/reference/isa/unpriv/rv64.html>
- Calling convention:
  <https://riscv-non-isa.github.io/riscv-elf-psabi-doc/>
- Hart-local instruction synchronization:
  <https://docs.riscv.org/reference/isa/unpriv/zifencei.html>

## Current measured opcode availability

See [jit64-status.md](jit64-status.md) for the reproducible profile comparison.
RV64I has87/173 guarded native paths, RV64I+Zmmul90/173 and RV64M103/173.
These are emission-availability counts, not performance percentages or complete
Dis64 runtime acceptance. Missing/declined RV leaves return to the existing VM
C dispatch; RV does not yet have the AMD64/ARM64 complete tail-entry table.
Complex allocation, scheduling, linking and ownership handlers should remain in C
when that is the appropriate implementation; a useful JIT need not duplicate them.
