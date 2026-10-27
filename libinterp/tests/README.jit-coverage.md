# 64-bit JIT opcode availability checkpoint

Latest packed descriptor/GC and native builtin cleanup checks: [README.gc-packed-maps.md](README.gc-packed-maps.md).

Latest AMD64/ARM64 CALL/MCALL bytecode-span checks and legacy native-target compatibility: [README.call-targets.md](README.call-targets.md).

Complete decoded JIT+C support now covers all 173 active commands on AMD64/ARM64. Native fast availability remains 166/173; seven complex handlers use C through complete tail entries. See [README.complete-dispatch.md](README.complete-dispatch.md) and the [per-command table](README.jit-readiness.md).

Latest LOAD callback capture, builtin return cleanup and C-only SPAWN/MSPAWN preflight: [README.module-transitions.md](README.module-transitions.md).

Latest stack spans, protected frame headers and captured allocation cursors: [README.stack-frame-contract.md](README.stack-frame-contract.md).

Latest native-safe instruction snapshots, indirect pointers and decoder regeneration: [README.decoded-addresses.md](README.decoded-addresses.md).

Latest canonical signed-word loading and complete AMD64/ARM64 cache validation: [README.dis-numeric-cache.md](README.dis-numeric-cache.md).

Native Linux build tools and concrete remaining compiler/frame/pointer ABI blockers: [README.native-bootstrap.md](../../utils/tests/README.native-bootstrap.md). These opcode counts do not establish a bootable 64-bit VM.

Latest codec/formatter slots, overlapping output restoration and seven-command C allocation capture: [README.string-conversion-capture.md](README.string-conversion-capture.md).

Latest array operand/header preflight, captured C output and 256-byte SLICELA qword kernels: [README.array-spans-kernels.md](README.array-spans-kernels.md).

Latest six list and seven channel constructor operand/callback fixes: [README.constructor-capture.md](README.constructor-capture.md).

Latest NEW/NEWZ/MNEWZ decoded cells and captured C allocation output: [README.record-slot-preflight.md](README.record-slot-preflight.md).

Latest FRAME/MFRAME spans, descriptor-map preflight and captured publication: [README.frame-allocation-spans.md](README.frame-allocation-spans.md).

Latest CALL/MCALL/RET native+C address spans, import-entry carry and RET error ordering: [README.control-transfer-spans.md](README.control-transfer-spans.md).

Latest priority AMD64/ARM64 binding/ABI, pure channel preflight and Limbo range fixes: [README.priority-dispatch-ranges.md](README.priority-dispatch-ranges.md).

Latest native buffered SEND/RECV and ring preflight: [README.buffered-transfer.md](README.buffered-transfer.md).

Full per-command AMD64/ARM64 percentages, definition and domains: [README.jit-readiness.md](README.jit-readiness.md) ([CSV](jit-readiness.csv)).

Latest MOVMP/HEADMP span guards and MOVTMP descriptor/map preflight: [README.mapped-preflight.md](README.mapped-preflight.md).

Latest six-command array operand/header guards and shared raw/mapped native copy kernels: [README.array-slots-copy-kernel.md](README.array-slots-copy-kernel.md).

Latest nine-command fixed preflight and AMD64/ARM64 qword copy kernels: [README.fixed-slots-copy-chunks.md](README.fixed-slots-copy-chunks.md).

Latest nine-command numeric preflight and full normal power-scale CVTFX domain: [README.numeric-slots.md](README.numeric-slots.md).

Latest nineteen-command integer branch/JMP preflight: [README.integer-branch-preflight.md](README.integer-branch-preflight.md).

Latest 24-command integer ALU/division preflight: [README.integer-slot-preflight.md](README.integer-slot-preflight.md).

Latest 18-command transfer/conversion preflight: [README.transfer-slot-preflight.md](README.transfer-slot-preflight.md).

Latest eight-command decoded-slot fixes: [README.list-slot-preflight.md](README.list-slot-preflight.md).

Latest ownership preflight fixes and validation: [README.owner-preflight.md](README.owner-preflight.md).

The production sizing emitters report 175 opcode slots. `INOP` is invalid and
`IECLR` is unused, leaving 173 active commands. AMD64 and ARM64 now have native
fast paths for 166 commands (95.95%); seven use C through complete JIT entries. RV64 profiles
retain their earlier counts. WORD32/native-pointer and legacy WORD64
configurations have the same opcode counts at this checkpoint.

| Backend/profile | Active commands | Native fast path | C implementation without fast path |
| --- | ---: | ---: | ---: |
| AMD64 | 173 | 166 | 7 |
| ARM64 | 173 | 166 | 7 |
| RV64 + M | 173 | 103 | 70 |
| RV64 + Zmmul | 173 | 90 | 83 |
| RV64I | 173 | 87 | 86 |

The preceding addition is a guarded ICASEC path for empty string tables and exact
lower-label owner identities or equal empty values at the first binary-search
midpoint of a table with up to 4,095 rows. Equal owners retain stringcmp's signed length/capacity checks; distinct empty owners
require zero length and nonnegative capacity. H skips header reads. Different
nonempty owners, unmatched midpoints, larger tables and legacy compiled tables
retain C. This is partial opcode availability,
not a general native string search. See [README.string-case-native.md](README.string-case-native.md).

ADDC now validates decoded slots, backward reference-cell addresses and complete
String headers before metadata reads on AMD64/ARM64. C mirrors slot/header
preflight and full Heap-origin validation. Tests use the existing 128-unit
private capacity; opcode counts are unchanged. See
[README.append-preflight.md](README.append-preflight.md).

CVTCW/CVTCL/CVTCF also preflight source/destination slots and String headers
before parsing or FP conversion. C validates outputs before ASCII termination
or Rune-cache preparation. Coverage counts are unchanged; see
[README.decimal-preflight.md](README.decimal-preflight.md).

INDC/INSC now preflight all scalar/pointer slots and complete String headers;
INSC also bounds its backward reference cell before metadata reads. C mirrors
slot/header checks and validates full Heap origins. Existing native availability
is unchanged. See [README.string-cell-preflight.md](README.string-cell-preflight.md).

SLICEC and LENC also preflight decoded slots. Slice owner checks bound exactly
the reference/len/max metadata, preserving payload-free prefixes; length queries
retain their length-field-only behavior. C slicec/slicer validate metadata before
access. See [README.slice-length-preflight.md](README.slice-length-preflight.md).

All six String predicates now preflight both owner slots and both C-nil owners
before metadata reads. Target slots are checked only on taken branches, retaining
false-target nonaccess and comparison-error priority. See
[README.string-branch-preflight.md](README.string-branch-preflight.md).

The preceding addition is ICONSMP for pointer-free mapped payloads up to 256 bytes.
Native code resolves a checked live Type, stages the old tail across allocation,
copies payload bytes and publishes descriptor/owner updates. Mapped and larger
records retain the protected interpreter constructor, which now rolls back
partially acquired head references without releasing the old tail. Duplicate
and saturated references are checked before each increment. See
[README.mapped-cons-native.md](README.mapped-cons-native.md).

The preceding additions are INEWCM and INEWCMP. Native code captures complete
source/capacity words, calls protected shared allocators and publishes fully
initialized raw/mapped channel owners. Raw widths are stored natively before
publication. Mapped construction resolves live module descriptors in the runtime
helper. Existing-owner replacement retains C. Both buffered and unbuffered mapped
descriptors now receive pointer-map checks before allocation. See
[README.payload-channel-native.md](README.payload-channel-native.md).

The preceding additions are INEWCB, INEWCW, INEWCL, INEWCF and INEWCP. Native
capacity/empty-owner checks and delayed output publication surround the shared
channel allocator; existing-owner replacements retain C. Complete WORD lengths,
aliases, error cleanup, builtin Type references and native calling conventions
are exercised by C/RX and AMD64/ARM64 instruction-emulation drivers. Shared C
preflight also rejects invalid argument spans and dead replacement owners before
allocating channel resources. See [README.scalar-channel-native.md](README.scalar-channel-native.md).

The preceding additions are ICONSM and ICONSP on AMD64/ARM64. ICONSM stages its
previous owner across allocation and copies at most 256 raw bytes before output
publication. ICONSP validates ownership before allocation, then retains and
marks its captured pointer natively using live GC cells and the builtin Tptr
identity. Both call the shared cons allocator. Invalid or larger raw cases keep
the C path. Heap-prefix checks also reject a header beginning at nil across
record/array construction and byte/string conversions. See
[README.payload-cons-native.md](README.payload-cons-native.md).

The preceding additions are ICONSB, ICONSW, ICONSL and ICONSF on AMD64/ARM64.
They capture scalar bits in native code, call the shared cons allocator through
a checked runtime ABI, then store the captured payload natively. This uses small
aligned native stack frames, preserves aliases and retains C allocator/GC/error
behavior; it does not replace pool allocation with a native allocator. See
[README.scalar-cons-native.md](README.scalar-cons-native.md).

The preceding additions are INEWA and INEWAZ on AMD64/ARM64. Unique owning arrays
with the same requested type and length can reuse pointer-free payloads of at
most 256 bytes (element width at most 256). NEWA leaves scalar bytes unspecified;
NEWAZ clears them after complete owner/data-span checks. Empty and zero-width
arrays skip data access. Shared owners, views and mapped elements retain C.
See [README.array-constructor-native.md](README.array-constructor-native.md).

The preceding addition is INEW on AMD64/ARM64, sharing checked descriptor/owner
lookup with INEWZ/IMNEWZ. Unique standard pointer-free records of at most 256
bytes reuse their owner without accessing unspecified scalar bytes. Mapped,
shared and custom-finalizer cases retain ordinary allocation. NEWA/NEWAZ now
use the checked heaparray path, validating maps before allocating and preserving
full-WORD counts before conversion to int. See
[README.raw-record-arrays.md](README.raw-record-arrays.md).

The preceding additions are INEWZ and IMNEWZ on AMD64/ARM64. They reuse unique
standard pointer-free objects of 0 through 256 bytes, resolving the live module
type table and checking both finalizer identities. Custom finalizers, pointer
maps, shared owners and larger payloads keep the C path. The interpreter reuses
larger eligible objects as well. See [README.zero-object-native.md](README.zero-object-native.md).

The earlier ISLICELA expansion covers arbitrary valid destination indexes for
non-pointer byte copies of at most 128 elements on AMD64/ARM64. It checks the
complete element geometry (index + length <= INT_MAX) and native address spans
before overlap-aware copying. Validated empty copies retain C's wider WORD-index
behavior without inspecting type or data metadata. Other element types, mapped
owners and larger copies retain C. No opcode IDs were added in this expansion.
See [README.offset-slices-channels.md](README.offset-slices-channels.md).

The preceding additions are guarded IMOVMP and IHEADMP paths on AMD64/ARM64.
They resolve the current Module.type table, accept pointer-free records of
1 through 256 bytes, and preserve overlapping memmove order. Zero-size records
are native no-ops after required list/type validation, independently of pointer
map flags or temporary source reference counts. Mapped and larger records keep C.
See [README.mapped-copy-native.md](README.mapped-copy-native.md).

The preceding additions are guarded IMOVP, ISELF, IHEADP and ITAIL paths on
AMD64/ARM64. Empty destinations retain a live source; destinations already
holding that source skip the net-zero retain/release. Both paths now implement
Setmark directly, including propagator-color sources and the live nprop flag.
Nil sources remain valid for pointer moves and list payloads, but not ISELF.
Other owner replacements and invalid reference counts retain C.
The compiler supplies actual native heap offsets, reference width and the live
collector color/propagation addresses and propagator constant. No mutable
collector state is frozen at compilation time. See
[README.gc-owner-srv.md](README.gc-owner-srv.md) for the latest differential
evidence. This broadens four existing opcode paths without changing the counts.

The preceding additions are ICASE and ICASEL on AMD64/ARM64 for ordinary numeric
tables with 0 through 4095 rows. Native binary search validates module/table/
target metadata before committing a Dis PC; larger and relocated tables keep C.
The preceding IGOTO addition reads compiler-emitted four-byte
table entries, validates the count/index/target and commits a Dis PC. Relocated
native-PC tables retain C handling. Earlier additions include IEXPF, using the same repeated-squaring
sequence as C, including negative and minimum-width exponents. Earlier additions
include guarded ICVTFW, ICVTFL and ICVTFX paths.
The real-to-integer paths retain C for non-finite and large inputs; ICVTFX
accepts every normal exact signed power of two from 2^-1022 through 2^1023 when non-unit
scaling keeps the input and product normal. Unit scales retain their earlier
zero/subnormal support. Non-finite, underflow/overflow and unsupported-scale
cases retain C. CVTFW/CVTFL/CVTFX now accept
absolute source values below 2147483647.5 for a 32-bit result, or below the
binary64 value 2^63-1024 for a 64-bit result. These symmetric bounds leave room
for the half-unit correction in every rounding mode; remaining boundary and
error cases retain C. Earlier additions include IADDF, ISUBF,
IMULF, IDIVF and ICVTXF.
They preserve host floating rounding and sticky exception state; numerical C
helpers remain in use for complex operations.

The latest operand-coverage expansion removes value guards from ICVTWF, ICVTLF,
ICVTRF and ICVTFR on AMD64/ARM64. Native signed integer conversion and real
resizing now cover their complete input representations with the platform FP
environment. RV64 retains its exact-bit guarded implementations. No opcode IDs
were added in that expansion.

Signed division by -1 now stays native in AMD64 ordinary DIV/MOD and in the
AMD64/ARM64 fixed frame/coefficient paths. MIN/-1 preserves the VM wrap result;
ordinary remainder is zero. Zero divisors and other existing guards still
retain interpreter error handling. This expands operand coverage without adding
opcode IDs; RV64 fixed guards are unchanged.

These figures count opcode availability, not complete semantic or performance
coverage. Some native paths accept a bounded subset and return to C for other
operands. Rounded fixed operations now support VN-only scales from -63 through
63 and WN/VN+WN right shifts from -63 through -1 on AMD64/ARM64. WN/VN+WN
scales from 0 through 63 also stay native when the captured operand signs do
not require a rounding mask. Invalid-mask cases retain C error handling; the
scale guard now follows sign selection, without growing the emitter buffers. RV64 still declines nonzero rounding flags.
Some floating conversions also retain C for their range/error contract. Instructions may therefore contribute to both native execution
and fallback execution at runtime. No fully-native opcode count is asserted.

All six AMD64/ARM64 real branch predicates now use hardware floating comparison
for non-NaN operands, including subnormals. This follows MXCSR DAZ/FTZ and ARM64
FPCR FZ settings and preserves floating status. Raw NaN guards remain before any
floating operation and retain C comparison/error behavior. RV64 keeps its earlier
normal/zero/infinity guards. This expands operand coverage without new opcode IDs.

The remaining commands are a list of current interpreter dispatch paths,
not a requirement to hand-emit an additional implementation for every opcode. Complex C
operations already become native machine code through the platform C compiler.
A block JIT should call these helpers with a defined contract for arguments,
GC roots, exceptions, frame state and scheduler transitions. Allocation,
reference ownership, strings, channels and scheduling especially benefit from
keeping one shared, optimized C implementation. Native generation is most
useful for frequent simple operations and for removing dispatch between them.

The current 64-bit implementation remains a decoded-operation leaf JIT. The
C decoder calls a leaf for an instruction; a rejected leaf returns to the
existing C handler. This series does not implement direct helper calls from
multi-instruction native blocks. Remaining architecture work includes native
block construction, helper ABI transitions and a coordinated compiler/loader/
frame/GC pointer ABI. Finishing that work cannot be measured by counting only
opcode cases.

The later stack-lifecycle/multiply series adds `IMULB`, `IMULW` and `IMULL`
on base RV64I, raising its native-path count from 83 to 86. A subsequent exception-frame
series adds `IMULX` with its full signed 64-bit intermediate, bringing RV64I
to 87.

## Reproduce the counts

```
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/jit-coverage.py --json
```

The script compiles the actual emitters, queries every numeric opcode using
valid private layouts and the production code capacity, and checks the parsed
enum length against `MAXDIS`. JSON includes the exact supported/remaining
names for all three architectures, both WORD widths and each RV64 profile.
It requires a host C compiler, but no target SDK or Unicorn installation.
Its layout values represent valid emitter contracts; native adapter offsets
and RX publication are covered separately by `leaf64-runtime.py`.

## Current interpreter-only commands on AMD64/ARM64

The production sizing emitters report these nine active commands for both WORD models:

```
IALT INBALT ISPAWN ILOAD IMSPAWN IEXIT ISEND IRECV IRAISE
```

The list excludes invalid/unused slots and is regenerated from jit-coverage.py.
RV64+M additionally retains all six string comparison branches on the interpreter
path. Native availability remains guarded and may cover only some operands.

## This series

`ITCMP` now has a native path on all three backends, including RV64I. It
captures both native owner slots, accepts the language nil source, rejects
C-null/invalid owners to the existing VM error path, and compares immutable
heap type pointers. It changes no VM memory, reference counts, GC colors,
PCs, frame links or FP state. A private header displacement comes from the
compiler's actual Heap layout, independently of WORD width. The public Heap,
REG and Dis layouts are unchanged.

```
LEAF64_CASE=typecheck python3 libinterp/tests/leaf64-jit.py
LEAF64_ARCH=riscv64 RV_FEATURE_PROFILE=i LEAF64_CASE=typecheck python3 libinterp/tests/leaf64-jit.py
REFERENCE_NATIVE=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/reference-slots.py
```

Unicorn matrices cover distinct owners with matching/mismatching type
pointers, nil/null owners, aliased slots, high native pointers, supported slot
alignments, malformed layout rejection and unchanged memory/header images.
The native AMD64 reference fixture separately checks production C behavior,
mark/reference effects and unchanged frame state on both WORD models.

Stack destruction and diagnostic validation now read frame metadata and
mapped owner slots using byte copies. The regression fixture builds four
frames across two real Stkext allocations and varies both ordinary frame
alignments, module switches and nil extension-root type descriptors. It
executes actual `freeptrs`, `memchk`, `validstk` and `destroystack`, checks exact
owner releases and newest-first extension deallocation, and preserves all
ordinary frame bytes.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/stack-unwind.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

There are 4096 unwind cases per WORD model. `UNWIND_XEC_SOURCE`,
`UNWIND_VALID_SOURCE` and `UNWIND_CHECK_SOURCE` independently select alternate
stack destructor, frame validator and mapped scanner implementations. Selecting
the parent implementations reproduces each unaligned access under UBSan.
Physical ARM64/RV64 execution and complete OS/VM acceptance remain separate.

## String comparison checkpoint

AMD64 and ARM64 implement `BEQC/BNEC/BLTC/BLEC/BGTC/BGEC` for byte strings,
Rune strings, mixed representations and language nil. They compare unsigned
bytes and the C comparator's signed int Rune keys, use explicit lengths
(including embedded NULs), and read a native target only when the predicate is
true. Null owners and INT_MIN lengths decline before changing PC or VM storage.
Identical owners compare equal without scanning payloads. At that checkpoint,
counts were 109/64:
this broadens six existing native paths rather than adding six new opcode IDs.
RISC-V string branches remain on the interpreter path. Physical ARM64 testing
and full native-block/pointer-ABI completion remain separate work.

## Channel ring width and identical-range checkpoint

[Channel ring width](README.channel-ring-width.md) documents native-width
buffer offsets, overflow-free ring wrapping and null-buffer send failures.
AMD64 SLICELA now returns without stores for identical validated physical
ranges. The ARM64 loop and existing 64-unit emission limit are retained.
No opcode count changed: AMD64/ARM64 128/45, RV64+M 103/70,
RV64+Zmmul 90/83, RV64I 87/86 (native-path/interpreter-only).

## Short decimal conversion checkpoint

[Decimal native paths](README.decimal-native.md) add CVTCW and CVTCL to AMD64
and ARM64 for optional-sign ASCII values with at most nine digits. Larger,
Rune and general prefix parsing retain C fallback. Counts are now 130 native
paths / 43 interpreter-only instructions on each of AMD64 and ARM64;
RV64+M 103/70, RV64+Zmmul 90/83 and RV64I 87/86 are unchanged.

## Existing-character store checkpoint

[String stores and capacity validation](README.string-store-native.md) add
INSC for unique ASCII/Rune owners with an existing character index. Appending,
shared-string copies and representation promotion retain C fallback. The
private capacity offset also keeps decimal conversion guards consistent with
C string2c. AMD64 and ARM64 now each have 131 native paths / 42 interpreter-only
active instructions. RISC-V profile counts remain unchanged.

## Real decimal and append checkpoint

[Real decimals, append stores and comparison capacity](README.real-decimal-append.md)
adds CVTCF for bounded decimal integers on AMD64/ARM64, extends INSC to append
without allocation, and validates both string capacities before native
comparisons. Both backends now provide 132 guarded native paths / 41
interpreter-only instructions out of 173 active instructions. RISC-V profiles
remain 103/70 with M, 90/83 with Zmmul and 87/86 for the base integer ISA.
These counts describe available guarded paths, not full operand or VM ABI coverage.

## ASCII append and wider decimal checkpoint

[ASCII append and wide decimal parsing](README.ascii-append-wide-decimal.md)
adds ADDC for unique ASCII destinations with spare capacity and expands the
64-bit integer/real decimal parsers. AMD64 and ARM64 now have 133 guarded native
paths / 40 interpreter-only instructions out of 173 active commands. RISC-V
profiles remain 103/70 with M, 90/83 with Zmmul and 87/86 for the base integer ISA.
Full operand coverage, physical target execution and the complete native VM ABI
remain separate from these guarded instruction counts.

## Rune concatenation and Type retention checkpoint

[Rune concatenation and allocation lifetimes](README.rune-append-type-retains.md)
expands ADDC to matching Rune representations on AMD64 and ARM64 while keeping
its 64-unit limit. Native opcode counts remain 133/40 on both backends; RISC-V
profiles remain 103/70, 90/83 and 87/86. Descriptor lifetime checks now precede
scalar heap, VM array and Dis-loader array allocation.

## Unique string slice checkpoint

[Unique string slicing and string descriptor retention](README.string-slice-native.md)
adds SLICEC to AMD64/ARM64 for bounded nonempty slices of unique ASCII/Rune
owners and gives the C instruction the same allocation-free reuse behavior.
Both backends now provide 134 guarded opcode paths / 39 interpreter-only active
instructions. RISC-V profile counts remain 103/70, 90/83 and 87/86. String
constructors now preflight Tstring retention; slicing and duplication reject
dead string owners before allocation or destruction.

## Large prefix and direct descriptor checkpoint

[Descriptor preflight and large string prefixes](README.descriptor-prefix-preflight.md)
extends AMD64/ARM64 SLICEC to validated zero-start prefixes without a copy-size
limit. Direct list, byte-array and module-link constructors now reserve their
Type references before allocation/publication. Opcode availability is unchanged:
AMD64/ARM64 134/39; RV64+M 103/70; RV64+Zmmul 90/83; RV64I 87/86.

## Pointer-free frame checkpoint

[FRAME and channel descriptor reservation](README.frame-native.md) adds FRAME
for pointer-free types that fit strictly within the current stack extent on
AMD64/ARM64. Both now provide 135 guarded opcode paths / 38 interpreter-only
active commands. RISC-V profiles remain 103/70, 90/83 and 87/86. Channel Type
preflight now accounts for buffer, mover and aliased header references before
creating any resource.

## Imported frame checkpoint

[MFRAME and malformed frame metadata](README.mframe-native.md) adds MFRAME for
nonnegative imports and pointer-free frames that fit in the existing extent.
AMD64/ARM64 now provide 136 guarded paths / 37 interpreter-only active commands;
RISC-V profiles remain 103/70, 90/83 and 87/86. C frame/stack checks reject
negative pointer-map lengths, and named-export lookup rejects an H table before
scanning names.

## Ordinary return checkpoint

[RET and validated stack teardown](README.return-native.md) adds same-module,
pointer-free RET on AMD64/ARM64. They now provide 137 guarded paths / 36
interpreter-only active instructions; RISC-V profiles remain 103/70, 90/83 and
87/86. C return and unextend validate saved owners before publishing register
changes or performing cleanup. The parallel ctypes fixture repair is retained.

## Same-module call checkpoint

[MCALL and module-owner preflight](README.mcall-native.md) adds nonnegative,
same-module bytecode MCALL on AMD64/ARM64. Both now provide 138 guarded paths /
35 interpreter-only active commands. RISC-V profiles remain 103/70, 90/83 and
87/86. C MCALL checks module references and entry targets before frame or REG
publication; private layout and ctypes probes are updated together.

## Equal-mode cross-module call checkpoint

[Cross-module MCALL and direct Type preflight](README.cross-call-retains.md)
extends AMD64/ARM64 MCALL to different bytecode modules with equal compiled
flags. Both remain at 138 guarded paths / 35 interpreter-only commands; RISC-V
profiles remain 103/70, 90/83 and 87/86. Module-data clones and per-word token
nodes now reserve descriptor capacity before allocating or publishing owners.

## Wider record copies and mapped clone ownership

[Mapped clone ownership and record-copy expansion](README.clone-owners.md)
extends pointer-free MOVMP/HEADMP records to 256 bytes on AMD64/ARM64 while
keeping the 64-unit staging bound. Counts remain 138/35 on both backends.
Mapped module-data cloning now validates pointer spans and owner capacity,
uses byte-safe field access, and leaves only acquired owners for cleanup.
Tokenization validates both input capacities before reading either payload.

## Same-mode module return checkpoint

[Native module returns](README.module-returns.md) extends AMD64/ARM64 RET to
pointer-free ordinary frames with retained same-mode module owners, including
self-module aliases. Last-reference releases, mode changes and frame cleanup
still use C. Owner preflight prevents C RET from publishing REG before discovering
a dead module. Counts remain 138/35 on AMD64/ARM64 and 103/70, 90/83, 87/86 on
RISC-V profiles. Mixed C/RX MCALL/RET round trips check exact reference restoration.

## Unique array-view checkpoint

[Unique array-view slices](README.array-view.md) adds SLICEA to AMD64/ARM64 for
allocation-free narrowing of unique views, including mapped and zero-width
elements. Both now provide 139 guarded native commands / 34 remaining. Shared
views and root arrays keep C allocation/ownership semantics. Layout adapter,
ctypes probes and publication/fallback fixtures are updated together.
[Self-load retain checks](README.self-load-retains.md) also prevent Module/MP
counter overflow before linking, without changing failure ownership contracts.
RISC-V profiles remain 103/70, 90/83 and 87/86.

## Unique pointer-free root checkpoint

[Unique array slices](README.array-view.md) extends AMD64/ARM64 SLICEA to unique
pointer-free roots, preserving original allocation ownership through repeated
narrowing, copying and release. Mapped roots and shared owners retain C paths.
[Array-copy source validation](README.array-copy-source.md) rejects dead owners
and invalid nonempty byte spans before allocation. Counts remain 139/34 on
AMD64/ARM64 and 103/70, 90/83, 87/86 on RV64 M/Zmmul/I. ARM64 execution is
emulated; guarded command counts do not imply complete opcode semantics.

## Signed integer formatting checkpoint

[Integer-to-string formatting](README.integer-format.md) adds CVTWC and CVTLC
on AMD64/ARM64 for unique ASCII destinations with at least 24 bytes of capacity.
Both now provide 141 guarded commands / 32 remaining. The C paths reuse the same
storage and reject dead owners or invalid output metadata before allocation.
Shared/Rune/small/H destinations retain construction paths. RV64 M, Zmmul and I
remain 103/70, 90/83 and 87/86. Both WORD models share these counts; ARM64
instruction execution is emulated.

## Exact-capacity formatting and conversion lifetime checkpoint

[Integer formatting](README.integer-format.md) now counts sign/digits before
publication on AMD64/ARM64, admitting unique ASCII destinations down to exact
one-character capacity. ARM64 shares a rejection stub; both leaves keep the
64-unit limit without layout/ABI changes. Counts stay 141/32 on both backends,
and RV64 M/Zmmul/I stay 103/70, 90/83, 87/86.
[Conversion-source preflight](README.conversion-sources.md) rejects dead
string/array/root owners and native byte-span wrap before reading or allocating;
the public c2string helper also validates native spans independently.

## Guarded raw-copy span checkpoint

[Raw-copy spans](README.raw-copy-spans.md) adds source/destination native range
checks to C MOVM/HEADM and AMD64/ARM64/RV64 leaves, then broadens the native
copy loops from 128 to 256 bytes. HEADM validates payload-offset formation
before data access. Empty-copy and owner/count error precedence remain intact.
All three backends retain the 64-unit limit without layout/ABI changes.
Counts remain AMD64/ARM64 141/32 and RV64 M/Zmmul/I 103/70, 90/83, 87/86.

## Cycle-safe bounded list-length checkpoint

[Cycle-safe list lengths](README.list-length.md) adds allocation-free Brent
cycle checks to C LENL and AMD64/ARM64/RV64 leaves. Native scans expand from
128 to 256 nodes while preserving write-free fallback, one link read per node
and the 64-unit bound. C also bounds the full unsigned count before publication.
[Raise lifetime preflight](README.raise-lifetime.md) rejects dead/saturated
exception owners and invalid message Types before preparation or publication.
Counts stay AMD64/ARM64 141/32 and RV64 M/Zmmul/I 103/70, 90/83, 87/86.

## Rune-buffer formatting checkpoint

[Reusable Rune formatting](README.rune-format.md) extends AMD64/ARM64 CVTWC and
CVTLC to unique Rune allocations with representable byte capacity. New start/end
address guards precede converted max publication. Backwards final-position digit
emission keeps the 64-unit bound without layout/ABI changes. C mirrors ownership
and capacity semantics. Counts stay AMD64/ARM64 141/32 and RV64 M/Zmmul/I
103/70, 90/83, 87/86.

The real-format checkpoint adds guarded ICVTFC paths on AMD64/ARM64 for +0
and signed powers 2^0 through 2^19. Other real values retain C formatting.
See [README.real-format.md](README.real-format.md) for ownership, FP-state and
failure-ordering checks. The core checkpoint now passes 99 host drivers.

## Checked append spans checkpoint

[Checked append spans](README.append-spans.md) adds complete native address-range
checks to C addstring and AMD64/ARM64 ADDC, then expands matching ASCII/Rune native
appends from 128 to 256 source characters. Adjacent and nonadjacent metadata
remain within the 64-unit limit. New C reduced-address and emulated top-address
regressions fail on the parent implementations. The core checkpoint now has
100 host drivers. Counts remain AMD64/ARM64 142/31 and RV64 M/Zmmul/I 103/70,
90/83 and 87/86.

## Checked string cells and slices checkpoint

[Checked cell and slice addresses](README.string-cell-spans.md) adds accessed-span
checks to C INDC/INSC/slicer/SLICEC and AMD64/ARM64 leaves. Moving SLICEC expands
from 128 to 256 characters, while unique zero-start prefixes retain their
length-only path. Parent regressions demonstrate failures in each old native
opcode path and in five C modes. All paths remain below 64 units without an ABI
change. The core suite now contains 101 host drivers. Counts remain
AMD64/ARM64 142/31 and RV64 M/Zmmul/I 103/70, 90/83 and 87/86.

## Full signed decimal range checkpoint

[Signed decimal ranges and UTF-8 views](README.decimal-ranges.md) expands
AMD64/ARM64 CVTCW/CVTCL to signed extrema within 10/19-digit sequences. CVTCF
retains its exact 15-digit domain. All three leaves check their trailing-NUL
address span; C string2c checks ASCII termination and Rune storage before cache
preparation. Production lib9 big parsing and libmath real parsing pass separate
native comparisons. The core suite now contains 102 host drivers. Counts remain
AMD64/ARM64 142/31 and RV64 M/Zmmul/I 103/70, 90/83 and 87/86.

## Checked string-comparison spans checkpoint

[Checked comparison spans](README.string-compare-spans.md) validates C stringcmp's
accessed common prefix while retaining payload-free identities and empty
comparisons. All six AMD64/ARM64 predicates reject the final 16 GiB owner window
before native payload access; C provides precise handling there. False targets
remain unread, and both private layouts stay within 64 units without ABI changes.
The core checkpoint now has 103 host drivers. Counts remain AMD64/ARM64 142/31
and RV64 M/Zmmul/I 103/70, 90/83 and 87/86.

The RV64 string-window checkpoint adds pre-metadata address-window rejection
for IINDC/ILENC and rejects IINDC payload layouts overlapping the length field.
Scalar-list HEADB/HEADW/HEADL/HEADF paths also validate offset and access-end
address sums before payload reads. The 50,145-case fixture covers both WORD widths and keeps
its checks with optimized Python. Opcode availability is unchanged. See
[README.riscv-string-window.md](README.riscv-string-window.md).

Numeric formatting now validates exact Tstring identity before native buffer reuse;
generic ADT/array pointer temporaries decline into the ownership-safe C conversion.
AMD64/ARM64 reserve 80 words for emission, including the full-width type guard.
[Regression and lifetime contract](README.format-pointer-temporaries.md).

The AMD64/ARM64 scalar-view checkpoint validates HEADB/HEADW/HEADL/HEADF, LENL
and LENC source spans before reads and matches these checks in C. Native LENL
now covers lists through 1024 nodes on these two backends, with unchanged early
cycle rejection and no partial publication. RV64 keeps its 256-node budget.
Opcode counts are unchanged. See [README.scalar-view-spans.md](README.scalar-view-spans.md).

CVTAC now has an ASCII byte-array reuse path on AMD64/ARM64 (1..256 bytes).
All guards precede mutation, including exact builtin source/destination types,
borrowed descriptor geometry and complete payload spans. The C path also reuses
unique ASCII buffers, retaining generic old owners through allocation failure.
Private staging is 128 words and the compiler supplies the new builtin array
identity. See [README.byte-string-native.md](README.byte-string-native.md).

CVTCA now has a byte-backed String export path on AMD64/ARM64 (1..256 bytes).
It reuses unique root-H byte Arrays with sufficient current length, validates
borrowed descriptor references and all spans before copying, and preserves
memmove/owner publication order. C additionally checks full Rune source spans
and supports empty/larger byte-backed reuse. See
[README.string-bytes-native.md](README.string-bytes-native.md).
