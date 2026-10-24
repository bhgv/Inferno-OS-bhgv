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
Scalar list heads are native, as are list length scans through 128 nodes; longer scans retain the VM helper.
Binary64 negation is native: only the sign bit changes, preserving signed zeros,
infinities and NaN payloads without requiring floating-point registers. Real
comparison branches handle normal values, zeros and infinities natively, while
NaNs/subnormals retain the C comparison path.
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
174,120 checks compare against independent Python integer/branch oracles for both
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
three-architecture machine-code suite now has 174,120 checks; the dedicated VM
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
checks RV32 compatibility and reports 2,048 checks. Floating addition and
conversion still use their existing VM implementations.
Real comparisons use the guarded leaves described below.

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

The copy subset reports 3,618 checks including encoder checks. It compares all
4 KiB of surrounding storage against snapshot-copy oracles for overlapping,
identical and disjoint spans, byte alignment, overwritten count operands,
128/129 boundaries, signed extrema and unmapped source/destination spans on
rejection. The string subset reports 852 checks including encoder checks, with
four-byte String.len offsets, both word widths, source/result aliases and
unchanged destinations on fallback. The complete driver reports 174,120 checks.
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

## List heads and bounded length scans

IHEADB/IHEADW/IHEADL/IHEADF now read inline List.data natively on AMD64,
AArch64 and RV64. Header offsets come from the production layout; the emulator
fixture deliberately uses a different data offset. Owners are captured before
possibly aliased output writes. H and C-null owners decline without changing
storage, allowing optab to raise exNilref. Scalar real heads copy all binary64
bits, including signaling NaN payloads, without evaluating floating arithmetic.
Pointer/mapped heads and tails keep their interpreter reference ownership paths.

ILENL scans at most 128 nodes in each 64-bit backend. H terminates the list and
returns the count, including zero for an empty list. C-null links or a scan that
would exceed 128 nodes return zero without storing a partial result; optab then
runs the normal length helper. The explicit List.tail offset is independent of
the inline payload offset. This bound also prevents the native leaf from
spinning on cycles; the existing VM helper does not gain cycle detection.
RV32 retains interpreter dispatch for these new operations.

VM head instructions now capture owner pointers with byte-safe copies, reject
C-null along with H and copy scalar results with memmove. Raw head sizes and
mapped type indices are captured without native WORD alignment assumptions.
The length helper checks C-null at every link and publishes a result only after
a complete scan, using byte-safe operand access. Reference-valued head/tail
instructions share the owner check while preserving their existing movers.

```
LEAF64_CASE=head python3 libinterp/tests/leaf64-jit.py
LEAF64_CASE=listlen python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/lists.py
```

The full emulator driver reports 174,120 checks. Head checks cover byte/word/big/
real bit patterns, high native pointers, source/result aliases, partial overlap
with inline payloads and unchanged surrounding object/operand bytes. Length
checks cover both word widths, 0/1/127/128/129 boundaries, null links, cycles,
source/result aliasing and no writes on fallback. Native AMD64 tests execute
both emitted code and forced real VM helpers for identical head/length inputs;
129/130-node lists additionally exercise native rejection followed by C fallback.
Both native word models pass ASan/UBSan.

The VM list fixture reports 3,434 cases per word model, including unaligned
owner/result/count/type-index operands, raw copy overlap, null failures, 129-node
scans and reference counts when headp/tail replace the list that owns the result.
Against parent 1d8a3e3, `LIST_SOURCE` source substitution with
`LIST_HEAD_CASE=nil` reproduces a null head access, `LIST_HEAD_CASE=raw` a
misaligned WORD64 headm size load, and `LIST_HEAD_CASE=length` a null length-link
access. The repaired implementation passes ASan/UBSan. The compiler fold-traps
fixture extracts the new shared listhead helper so its actual VM comparison
continues to cover head/tail exceptions. Target hardware boots remain separate.

## String character indexing

IINDC now reads ASCII or Rune elements natively on AMD64, AArch64 and RV64.
String.len remains a signed 32-bit representation field even with WORD64.
The emitters reject nil/H owners, INT_MIN representation lengths and complete
out-of-range VM indices before reading character storage. ASCII bytes are
zero-extended; Rune values are interpreted as signed 32-bit integers before
publishing the VM word, matching the existing `(int)Srune[index]` rule. Header
and character-data offsets are explicit layout inputs derived from String.
These are scalar reads; string mutation and allocation remain in optab.

The VM indc and insc handlers now reject C-null owners and INT_MIN lengths with
exNilref/exHeap before any output or ownership changes. Operand words and owner
slots are captured with byte-safe copies, and results/new owners are published
without assuming native alignment. The insc slot itself stays a void pointer,
so an unaligned byte address is never converted into a typed String**.
Existing sharing, growth, rune promotion and allocation-failure behavior remains
covered by the codec tests.

```
LEAF64_CASE=character python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/string-codecs.py
STRING_INDEX_CASE=alignment python3 libinterp/tests/string-codecs.py
STRING_INDEX_CASE=stores python3 libinterp/tests/string-codecs.py
```

The complete JIT driver reports 174,120 checks. Character tests cover both word
widths, ASCII/rune strides, signed Rune extrema, high-word indices, nil/H owners,
INT_MIN metadata, output aliases with owner/index/payload and whole-buffer
preservation. Rejected indices also use an owner header at a mapped-page edge
with unmapped character storage. Native AMD64 tests compare actual emitted code
with forced production VM helpers, including errors and unchanged Dis PCs, under
ASan/UBSan in both word models.

The complete codec fixture reports 40,095 checks per word model under UBSan.
New focused read/store/error subsets also pass ASan/UBSan with stack checks
retained. Store coverage includes unaligned rune/index/owner slots, appending,
ASCII-to-rune promotion, negative rune values, shared owners and unchanged
storage on bounds failures. `STRING_SOURCE` plus `STRING_INDEX_CASE=nil`, `min`,
`store-nil`, `store-min` or `alignment` reproduces the corresponding null access,
INT_MIN negation or unaligned operand load against parent 692d358.

In this sandbox the full codec ASan process reports a stack-buffer diagnostic
inside the pre-existing cvtac/checkbounds longjmp sequence on both parent and
current sources. Thus a full codec ASan pass is not claimed. The complete UBSan
run, focused ASan subsets and native JIT ASan execution are the successful
checks; stack instrumentation was not disabled. Full operating-system boots and
hardware arm64/riscv64 execution remain separate.

## Native modular integer powers

IEXPW and IEXPL now emit helper-free exponentiation by squaring on AMD64,
AArch64 and RV64. The exponent is always a VM word, including for big results.
A nonnegative exponent needs at most one iteration per exponent bit. Zero
exponents return one, including zero to power zero. Multiplication preserves the
VM's modular low bits; WORD32 results are narrowed only at the final store.
RV64 requires M, and RV32/no-M configurations keep interpreter dispatch.

Negative exponents decline before reading the base or writing any output. The
VM then supplies the existing rules: zero base raises exZdiv, bases 1/-1 return
1 or the parity-selected sign, and other integer bases return zero. No partial
power is published. Both input values are captured before storing, including
when the destination overlaps the exponent or base.

The three interpreter power helpers now capture operands and publish results
with memmove rather than assuming native alignment. Integer wrap, negative
exponents, exceptions and floating reciprocal/rounding behavior are preserved.
IEXPF remains an interpreter operation; no floating power leaf was added.

```
LEAF64_CASE=power python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/leaf64-runtime.py
python3 limbo/tests/fold-effects.py
```

The full machine-code suite reports 174,120 checks; the focused power subset
reports 12,108 including encoder checks. Independent Python modular-pow oracles
cover both word models, full-width/high-word exponents, modular extrema, zero,
negative bases and exact/partial operand aliases. Negative exponents also use
an unmapped base operand to verify guard order and unchanged output/PC.

The native AMD64 fixture runs both emitted code and forced production helpers
against a high-bit-first modular oracle. It exercises negative exponent fallback,
zero-base exceptions, four-byte-aligned word/big slots, partial overlap and
unchanged neighbors/Dis PCs. Real powers remain C helpers and also test minimum
full-width exponents through unaligned operand/result slots. Both word models
pass ASan/UBSan with stack instrumentation active.

The compiler/VM fold-effects fixture reports 9,341 checks per word model. New
matrices compare word, big and real powers through unaligned operands and five
result placements, while retaining boolean-folding, reciprocal subnormal,
signed-zero/infinity and minimum-exponent checks. It passes ASan/UBSan; LSan
process inspection is disabled because the sandbox cannot inspect target
processes. This does not disable address or stack instrumentation. Parent
c15785e reproduces an unaligned WORD64 power load via LEAF_VM_SOURCE and an
unaligned LONG load in the WORD32 fixture via POWER_VM_SOURCE. Current sources
pass both. The RISC-V regression fixture reports 2,048 checks and explicitly
rejects power leaves without M or on RV32, and real-power leaves on every model.
Full Inferno boots and arm64/riscv64 target hardware execution remain separate.

## Raw list-head record copies

IHEADM now shares the bounded overlap-safe copy leaves on AMD64, AArch64 and
RV64. A head owner is captured and checked against H/C-null before reading the
count, even at length zero. Valid zero lengths return without touching payload
or destination storage. Negative counts and counts above 128 decline without
writes, leaving the VM bounds exception or optimized large memmove authoritative.
RV32 retains optab dispatch; RV64 does not require M for raw head copies.

```
LEAF64_CASE=headcopy python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/leaf64-runtime.py
```

The focused subset reports 2,592 checks including encoder checks. Whole-buffer
snapshot oracles cover both copy directions, exact/partial overlap, overwritten
count operands, aliases with the owner/count slots and the 128/129 boundary.
Nil/H owner tests also use an unmapped count; zero/rejected-count tests use an
unmapped destination. Native AMD64 tests compare emitted code and forced real
headm helpers against libc memmove, including large-record fallback and owner
validation before poisoned counts. Both word models pass ASan/UBSan.

The complete native-code suite reports 174,120 checks, and explicitly keeps
pointer/mapped heads and tails unsupported. Those operations continue to use
the VM ownership movers. Full target boots and hardware arm64/riscv64 execution
remain separate. See README.pointer-maps.md for the accompanying constructor,
heap pointer-map and reference-count changes.

## Guarded real comparison branches

IBEQF/IBNEF/IBLTF/IBLEF/IBGTF/IBGEF now emit native leaves on AMD64, AArch64
and RV64. Accepted operands are binary64 normal values, signed zeros and
infinities. Integer instructions normalize both zeros to one key and reverse
the low 63 bits of negative encodings; signed key comparisons then preserve
numeric order. No emitted instruction touches floating registers or FP status.
RV64 needs neither M nor floating-point instructions for these leaves; RV32
retains interpreter dispatch. No performance claim is made for this baseline.

NaN and subnormal operands return zero without changing memory, PC, rounding or
FP flags. NaNs must reach the existing C comparison for unordered results and
signaling behavior. Subnormals also stay in C so host flush/denormal-control
modes are not bypassed by mathematical bit ordering. A rejected first operand
is classified before accessing the middle value. A false comparison never reads
the target operand. Supported true comparisons publish the full native Dis PC;
Module.compiled, frames, GC maps and the decoder/quantum loop remain unchanged.

The VM wrappers share branchreal, which captures both operands with memmove and
loads the target only on a taken branch. This removes undefined native REAL and
Inst* alignment assumptions without changing the six numeric C predicates.
In particular, NaN inequality remains true and the other unordered predicates
remain false according to their existing C operators.

```
LEAF64_CASE=realbranch python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/real-branches.py
REAL_BRANCH_CASE=target python3 libinterp/tests/real-branches.py
python3 libinterp/tests/leaf64-runtime.py
```

The full JIT driver reports 174,120 checks. The focused real-branch run reports
25,839 including encoder checks: all six predicates, both word models, signed
zeros, normal boundaries, infinities, positive/negative quiet/signaling NaNs,
subnormals, random raw bits, high PCs and four-byte-aligned values/targets.
Rejected first operands also use an unmapped middle value, and rejected/false
branches use an unmapped target. Operand bytes and REG pointer fields remain
unchanged. The RISC-V regression fixture reports 2,048 checks and includes no-M
and RV32 emission/fallback checks for every new predicate.

The VM comparison fixture reports 50,752 checks per word model, 101,504 total.
It tests every byte alignment, two rounding modes, unchanged surrounding data,
false branches with invalid targets and pre-existing FP exception flags. Separate
volatile numeric operator oracles also compare exact C-path flags for NaNs and
subnormals, alongside the normal-value flag-preservation checks. The
native AMD64 fixture executes emitted leaves and forced real VM helpers against
numeric C oracles, checks exact native completion/fallback classification and
preservation of rounding/FP flags by every emitted leaf, and then verifies the
C fallback PC result. One rounding pass seeds FE_INEXACT; the other seeds all
standard FP flags. Both word models and the dedicated VM fixture pass ASan/UBSan
with stack instrumentation retained; LSan process inspection is disabled.

REAL_BRANCH_SOURCE substitutes xec.c. Parent 77b9d7d reproduces an unaligned REAL
load in the default fixture and an unaligned Inst* target load with
REAL_BRANCH_CASE=target. The corrected functions pass both cases. The core suite
now has 29 host drivers. Actual arm64/riscv64 hardware FP-state acceptance and
complete Inferno boots remain separate from these emulator and host checks.
