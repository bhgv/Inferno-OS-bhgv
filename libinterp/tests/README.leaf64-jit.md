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

The emitters handle byte/word/long/real bit-preserving moves, byte/short and
word/big conversions, pointer-width address moves, modular byte/word/big integer
arithmetic and bitwise logic, byte/word/big comparisons and jumps. All three
64-bit backends now support guarded byte/word/big shifts, division and remainder.
Invalid counts and zero divisors return zero without changing VM storage or PC,
so optab raises the existing VM exception. AMD64 also declines divisor -1 to
avoid hardware IDIV overflow; the VM supplies the defined minimum/-1 wrap or zero
remainder. ARM64 and RV64 implement that pair directly with architecture-defined
integer results. RISC-V division requires M; RV32 division remains in optab.
Array lengths are native in both word models; fixed byte/word/big/real element
addresses are native when VM words hold pointers. Header offsets are explicit
layout inputs derived from Array, and all rejected inputs use the VM helper.
Generic descriptor indexing remains in the interpreter. String lengths and raw
record moves of up to 128 bytes are also native; larger moves use optab/memmove.
Binary64 negation is native: only the sign bit changes, preserving signed zeros,
infinities and NaN payloads without requiring floating-point registers.
Other floating arithmetic, reference ownership, allocation, calls, returns, exceptions
and scheduling retain the existing decoder/optab implementations. Unsupported
operations return no code; no stub falsely claims an operation was completed.

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
120,660 checks compare against independent Python integer/branch oracles for both
VM word widths. It covers overflow boundaries, randomized values, exact input
aliases, partial move overlaps, four-byte-aligned long operands, high native
addresses, PC updates, neighboring-byte guards, sizing, capacity rejection and
unsupported-operation fallback. Rejected shifts and zero divisors additionally
use an unmapped middle operand, proving that validation precedes its value read.
Actual REG field offsets are tested separately
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
host test. Direct AMD64 execution requires an x86-64 host. The native fixture also
extracts actual divw/modw/shlw/shrw/lsrw and shiftcount helpers from xec.c. It
checks native completion or real VM fallback, minimum/-1, zero division, invalid
counts, unchanged outputs on exceptions and unchanged Dis PC with both WORD32
and WORD64 declarations (`LEAF_WORD_BITS=32` selects the narrow model).

The existing RISC-V fixtures retain their native encoder, publication/error,
module-switch, quanta and destruction checks. The core VM/compiler checkpoint
covers the shared Jit metadata change. The native AMD64 fixture passes UBSan and
ASan+UBSan; encoder fixtures use UBSan. Floating arithmetic, native callee-frame
transitions, exceptions through emitted code and complete Inferno boots on the
three targets have not been validated. No performance claim or full-JIT claim is
made. Tk, security/cryptography and MIPS/SPARC/PowerPC backends are unchanged.

## Guard-order and empty-alt regressions

The RISC-V emitter previously loaded the middle value before checking a shift
count. An invalid count with an unmapped value span faulted instead of returning
to the VM helper. All guarded leaves now validate their source before reading
the middle value or fetching the destination pointer. The focused test is:

```
LEAF64_ARCH=riscv64 LEAF64_CASE=guard python3 libinterp/tests/leaf64-jit.py
```

`LEAF64_RISCV_EMITTER` substitutes an alternate RISC-V header. The parent header
fails this subset with UC_ERR_READ_UNMAPPED; the corrected header passes for
both word models. Existing RISC-V publication tests now use floating arithmetic
as their deliberate unsupported opcode, since integer division is supported.

VM alt array registration, cancellation and communication now iterate by WORD
index. This removes null-plus-zero/end-pointer formation for empty array views
and avoids narrowing their selected index. The empty-guard fixture adds 28 cases
per runtime/word configuration (112 executions), covering nil/sentinel empty
data, multiple empty guards, default selection and a ready buffered scalar guard
at every position. It passes ASan+UBSan with stack instrumentation enabled:

```
CHANNEL_TRANSFER_CASE=empty CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/channel-transfers.py
```

The previous empty-alt output often matched; this is a C-definedness correction,
not a claimed independently reproduced sanitizer crash in that path. The shared
core checkpoint runs the extended channel fixture. No full target boot, native
ARM64/RV64 hardware run, complete frame ABI migration or relocated-PC JIT is
claimed by this extension.

## Array address extension

The array extension adds lena and native-pointer-width indb/indw/indl/indf to all
three 64-bit backends. See [the array-address guide](README.array-indices.md) for
the checked VM helper, zero-width semantics, native address guards, word-model
limitations and independent parent regressions. The array-only emulator matrix
adds 26,100 semantic executions plus shared sizing/ABI checks. The complete
three-architecture machine-code suite now has 120,660 checks; the dedicated VM
address fixture adds 40,023 checks. These counts do not imply whole target boot
or a new Dis64 ABI.

## Aligned control transfers and binary64 negation

RV64 jumps and taken comparisons now fetch their native target from a Dis slot
with two 32-bit reads. The slot is only guaranteed four-byte alignment; an LD
could trap on hardware even though Unicorn permits it. The false-branch skip
accounts for the longer target load. REG pointer fields must retain native
alignment, and malformed layout offsets are rejected during emission.
Both RISC-V execution fixtures now reject any misaligned native memory access.
The focused reproducer is:

```
LEAF64_ARCH=riscv64 LEAF64_CASE=control python3 libinterp/tests/leaf64-jit.py
```

With `LEAF64_RISCV_EMITTER` pointing to the parent header, the test fails on
IBEQB's eight-byte read at a four-byte-aligned destination. The corrected encoder
passes both word models. This is an alignment regression check, not a target
hardware boot result.

INEGF is emitted on AMD64, AArch64 and RISC-V by toggling the binary64 sign bit
using integer instructions. Tests cover both zeros, subnormals, infinities,
quiet/signaling NaN bit patterns, random bit patterns, exact aliases and partial
overlap. The native AMD64 compiler fixture executes the published code with real
REG offsets under ASan/UBSan in both word models. The RISC-V fixture additionally
checks RV32 compatibility and reports 1,995 checks. Floating addition, conversion
and comparison still use their existing VM implementations.

## Bounded record copies and string lengths

All three 64-bit leaf backends now emit IMOVM for lengths 0 through 128. They
capture the count before any output write, select a forward/backward byte loop
for overlapping spans and avoid touching source/destination storage at length
zero. Negative lengths return to the VM's bounds exception. Larger positive
lengths return without writes so the existing platform memmove handles them;
there is no performance claim for the small byte-loop baseline. Rejected lengths
are checked before source/destination pointers or data are accessed. RV32 leaves
IMOVM in the interpreter.

ILENC reads the signed 32-bit String.len field independently of VM word width.
Positive ASCII and negative rune counts yield the same positive VM length. H
produces zero. C-null owners and INT_MIN metadata decline to optab before an
output write. The VM now raises exNilref for C-null and exHeap for INT_MIN,
rather than dereferencing null or evaluating signed -INT_MIN. Header length
fields and operand widths are separate explicit layout inputs.

The VM movm helper captures its count with memmove instead of a WORD pointer
load. The lenc helper similarly captures its owner pointer and writes its result
with byte-safe copies. These avoid undefined native alignment assumptions for
Dis operands that are four-byte aligned in the WORD64 test model; they do not
establish a complete new Dis64 frame/GC ABI.

Focused emulator checks:

```
LEAF64_CASE=copy python3 libinterp/tests/leaf64-jit.py
LEAF64_CASE=string python3 libinterp/tests/leaf64-jit.py
```

The copy subset reports 3,510 checks including encoder checks. It compares all
4 KiB of surrounding storage against snapshot-copy oracles for overlapping,
identical and disjoint spans, byte alignment, overwritten count operands,
128/129 boundaries, signed extrema and unmapped source/destination spans on
rejection. The string subset reports 744 checks including encoder checks, with
four-byte String.len offsets, both word widths, source/result aliases and
unchanged destinations on fallback. The complete driver reports 120,660 checks.
The native AMD64 fixture compares copies with libc memmove, tests the real
String/REG offsets and runs actual VM fallback for length 129, invalid counts,
C-null strings and INT_MIN metadata. Both word models pass ASan/UBSan.

`STRING_SOURCE` and `LEAF_VM_SOURCE` allow production source substitution for
regressions. Against parent 79a4692, the runtime fixture diagnoses a misaligned
WORD64 movm count load. The string fixture with `STRING_LENGTH_CASE=nil` diagnoses
a null String member access, and `STRING_LENGTH_CASE=intmin` diagnoses signed
INT_MIN negation. The default string fixture additionally tests four-byte-aligned
owner/result slots and aliasing. The repaired sources pass those cases with
ASan/UBSan. None of these tests is a full target operating-system boot.
