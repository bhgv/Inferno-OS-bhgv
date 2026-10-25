# 64-bit JIT opcode availability checkpoint

The production sizing emitters report 175 opcode slots. `INOP` is invalid and
`IECLR` is unused, leaving 173 active commands. AMD64 and ARM64 now have native
paths for 128 commands (74.0%); 45 remain on the interpreter path. RV64 profiles
retain their earlier counts. WORD32/native-pointer and legacy WORD64
configurations have the same opcode counts at this checkpoint.

| Backend/profile | Active commands | Native path | C interpreter path |
| --- | ---: | ---: | ---: |
| AMD64 | 173 | 128 | 45 |
| ARM64 | 173 | 128 | 45 |
| RV64 + M | 173 | 103 | 70 |
| RV64 + Zmmul | 173 | 90 | 83 |
| RV64I | 173 | 87 | 86 |

The latest ISLICELA expansion covers arbitrary valid destination indexes for
non-pointer byte copies of at most 128 elements on AMD64/ARM64. It checks the
complete element geometry (index + length <= INT_MAX) and native address spans
before overlap-aware copying. Validated empty copies retain C's wider WORD-index
behavior without inspecting type or data metadata. Other element types, mapped
owners and larger copies retain C. No opcode IDs were added in this expansion.
See [README.offset-slices-channels.md](README.offset-slices-channels.md).

The preceding additions are guarded IMOVMP and IHEADMP paths on AMD64/ARM64.
They resolve the current Module.type table, accept pointer-free records of
1 through 128 bytes, and preserve overlapping memmove order. Zero-size records
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
accepts exact signed powers of two from 2^-16 through 2^16 when non-unit
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

```
IALT INBALT IFRAME ISPAWN ILOAD IMCALL IMSPAWN IMFRAME IRET IEXIT INEW INEWA
INEWCB INEWCW INEWCF INEWCP INEWCM INEWCMP ISEND IRECV ICONSB ICONSW ICONSP
ICONSF ICONSM ICONSMP IHEADP IHEADMP ITAIL IMOVP IMOVMP ICVTCA ICVTAC ICVTWC
ICVTCW ICVTFC ICVTCF IINSC IADDC ISLICEA ISLICELA ISLICEC ICVTLC ICVTCL ICONSL
INEWCL ICASEC IMNEWZ INEWZ INEWAZ IRAISE ISELF
```

The list excludes invalid/unused slots. RV64+M additionally retains all six
string comparison branches on the interpreter path. Many listed commands should remain C
helpers when they are integrated into a native block execution path.

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
