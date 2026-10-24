# AMD64, AArch64 and RV64 decoded-operation JITs

This checkpoint adds `comp-amd64.c` and `comp-arm64.c` and extends the existing
`comp-riscv64.c` backend. These are real native leaf JITs using the existing
Module.jit execution path. They preserve Dis instruction PCs and metadata;
Module.compiled remains zero because that flag denotes the older relocated-PC
ABI. This is a baseline JIT, not a replacement for every interpreter operation
or a complete new 64-bit operating-system/VM port.

AMD64 uses the Linux SysV register convention. ARM64 uses AAPCS64. Leaves return
one on completion, take REG* as their only argument and make no helper calls or
native stack frames. The AMD64 entry includes ENDBR64 and the AArch64 entry BTI c.
Both emitters support WORD32 and the current native WORD64 declaration with
64-bit native pointers. This does not choose a new canonical Dis64 ABI.

The new emitters handle byte/word/long/real bit-preserving moves, byte and signed
short conversions, word-sized address moves when words can hold pointers,
modular byte/word/big addition, subtraction, multiplication and bitwise logic,
byte/word comparisons and unconditional jumps. RV64 gains the six big arithmetic
and bitwise operations as well. Division, remainders, shifts, floating arithmetic,
reference ownership, allocation, calls, returns, exceptions and scheduling retain
the existing decoder/optab implementations. Unsupported operations return no code;
there is no stub that falsely claims an operation was completed.

Each module receives one leaf per distinct supported opcode. Runtime decoding
and the instruction-quanta loop still happen in xec. Leaves capture both inputs
before storing results, preserve adjacent data and update full native PC pointers.
They never modify GC maps, frames, branch tables or pointer-bearing record layouts.

Linux code is allocated with private mmap RW pages, synchronized using the
compiler cache-clear primitive and published only after mprotect changes them to
RX. Ordinary allocator/VM heap pages are never made executable. A Jit.freecode
callback lets freemod unmap the corresponding allocation; existing backends keep
the free-based ownership convention when that callback is nil. Descriptor,
mapping, protection and VM-error failures leave Module.jit unpublished. RV64 Linux
builds use the same mapping policy, while existing embedded RISC-V targets retain
their allocation/segflush path and per-entry fence.i behavior.

The object naming matches the existing comp-$OBJTYPE selection (`amd64`, `arm64`,
`riscv64`). Whole Linux amd64/arm64 emulator platform directories, system assembly,
builtin-frame ABI migration and platform make configurations are not added by
this checkpoint. Thus changing OBJTYPE alone is not a supported complete-port
build recipe. The backend objects can be syntax-checked with the current host
headers, and their native code can be tested independently as below. Non-Linux
AMD64/ARM64 code allocation fails closed rather than executing allocator memory.

## Verification

Install `unicorn` and `capstone` into the Python test environment, then run:

```
python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/leaf64-runtime.py
LEAF_RUNTIME_BACKEND=arm64 python3 libinterp/tests/leaf64-runtime.py
LEAF_RUNTIME_BACKEND=riscv64 python3 libinterp/tests/leaf64-runtime.py
python3 libinterp/tests/riscv-jit.py
python3 libinterp/tests/riscv-runtime.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

The cross-architecture test compiles the production encoders, disassembles every
emitted byte and executes AMD64, AArch64 and RV64 machine code in Unicorn. Its
53,388 checks compare against independent Python integer/branch oracles for both
VM word widths. It covers overflow boundaries, randomized values, exact input
aliases, partial move overlaps, four-byte-aligned long operands, high native
addresses, PC updates, neighboring-byte guards, sizing, capacity rejection and
unsupported-operation fallback. Actual REG field offsets are tested separately
by the native runtime compiler fixture; the emulator fixture deliberately uses a
compact layout to isolate encoder offsets and native pointer semantics.

On Linux x86-64, the runtime fixture invokes the complete production AMD64 compile
driver and executes its published RX machine code directly on the host CPU.
It checks descriptor allocation failure/VM error, mmap failure, mprotect failure/
VM error, cleanup, retry, opcode deduplication behavior, aliases, unsupported
entries, unchanged Dis storage and actual freemod unmapping. Mapping/protection
adapters call the real OS APIs and verify RW allocation/RX publication flags;
no /proc inspection is required. The same driver also checks ARM64 and RV64
publication/failure/unmapping paths on the host, with instruction execution left
to the emulator fixture; the RV64 mapping policy is explicitly enabled for this
host test. Direct AMD64 execution requires an x86-64 host.

The existing RISC-V fixtures retain their native encoder, publication/error,
module-switch, quanta and destruction checks. The core VM/compiler checkpoint
covers the shared Jit metadata change. The native AMD64 fixture passes UBSan and
ASan+UBSan; encoder fixtures use UBSan. Floating arithmetic, native callee-frame
transitions, exceptions through emitted code and complete Inferno boots on the
three targets have not been validated. No performance claim or full-JIT claim is
made. Tk, security/cryptography and MIPS/SPARC/PowerPC backends are unchanged.
