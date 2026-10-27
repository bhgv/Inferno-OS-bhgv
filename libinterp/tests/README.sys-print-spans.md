# Formatter native spans and borrowed string views

Sys formatting now bounds its output region before forming an end pointer.
Each consumed argument is checked before reading its complete representation:
32-bit Dis ints/widths, native String/debug pointers, REAL and LONG. Eight-byte
frame-relative alignment uses unsigned native coordinates, avoiding pointer
subtraction between unrelated objects and rejecting wrap before advancing.

A shared borrowed-string view captures len/max through byte copies and validates
logical capacity, header extent and the exact accessed payload. Format patterns
can be byte-aligned, including Rune patterns read through byte copies. `%s/%q`
arguments also reserve their trailing terminator before any store. The native
lib9 Rune formatter still requires Rune-aligned argument data; ASCII argument
headers can be packed. H and existing nil/INT_MIN error behavior remain intact.
An empty pattern needs no payload read. Already-emitted output is not rolled back
if a later argument fails; the new isolated rejection probes fail before output.
These numeric extent checks cannot establish arbitrary allocation membership.

`%H` retains the native unsigned Heap reference width rather than narrowing to
int. The language-nil diagnostic still prints -1. Backward Heap origins and
alignment are checked before reading debug metadata. `bigxprint` validates its
output pointer cell before allocation and publishes through memmove, including
unaligned cells. Protected formatting errors release the temporary buffer and
leave the result cell unchanged.

Argument advancement keeps the existing Dis packing rules; this is not a new
variadic compiler/runtime ABI. In particular native pointer reads and historical
Dis cursor steps are not asserted to form a bootable 64-bit varargs ABI.

## Validation

```sh
python3 libinterp/tests/sys-print-spans.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/sys-print.py
cc -fsyntax-only -I Linux/arm/include -I include -I emu/port -I libinterp libinterp/runt.c
```

The new default ASan/UBSan/NDEBUG matrix has 19,564 cases per WORD model, 39,128
across WORD32/WORD64 with native eight-byte pointers. It uses production xprint,
bigxprint, VM error guards and real lib9 formatting rather than a host printf
substitute. Linux pthread stacks make error unwinds visible to ASan even where
main-stack discovery through proc is unavailable. Instrumentation stays enabled.
The inherited formatter fixture now uses explicit predicates under NDEBUG, and
constructs valid String capacities. Its 1,014 cases per model remain passing.
The complete runt.c passes native syntax compilation against actual declarations.

| Added family | Cases per scalar model |
| --- | ---: |
| Output buffer wrap | 128 |
| Eleven argument forms, nil/H/wrap, both full/empty output capacities | 19,200 |
| Format/argument header and capacity errors | 26 |
| Packed ASCII/Rune patterns and argument views | 144 |
| Native Heap counts and invalid debug origins | 48 |
| Dynamic result slots, publication and protected failure cleanup | 18 |

Boundary arguments are rejected before any dereference; valid near-address-limit
memory is not mapped by this fixture. Counts through ULONG_MAX, caller/argument
images, output guards, allocation counts and balanced error frames are checked.
The wide-count oracle uses host integer rendering independently of lib9.

Twelve independent old-source controls fail across both scalar models and all
six families. Ten reach UBSan diagnostics; the two Heap-counter controls fail
an active numerical predicate because the old signed int diagnostic loses bits.
Replay with `SYS_PRINT_SOURCE`, `SYS_PRINT_MODELS=native|word32` and
`SYS_PRINT_CASE=buffer|arguments|metadata|packed|heap|growth`. The new driver is
registered in the core regression suite.

This series improves common C builtin paths without changing opcode availability:
AMD64/ARM64 remain at 173/173 complete JIT+C entries and 166/173 guarded native
fast paths (95.95%). It is not a physical ARM64 run or full native VM ABI release.
RV64-specific work, MIPS/SPARC/PowerPC JITs, Tk and security/crypto are unchanged.


## Incoming synchronization

Incoming `ff21d3c` is preserved unchanged. It corrects the branch-alias sentinel
oracle and restores extracted branchpc/JMP/exModule dependencies in arithmetic
and CALL fixtures, without changing production emitters. The eight tested
staged images in this series remained identical across the fast-forward/apply.
The incoming branch matrix passes 101,968 cases per AMD64/ARM64 backend;
C division/real-branch checks pass 2,640 WORD32 and 2,832 WORD64 cases, and CALL
slot checks pass 2,576 per scalar model. Guest ARM64 is not physical execution.
