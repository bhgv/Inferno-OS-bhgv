String case table regression
============================

Run `python3 libinterp/tests/string-case-c.py`. The core checkpoint includes it.
The driver extracts production `casec`, `caseccell`, `casepc` and `stringcmp`;
it also extracts ICASE/ICASEL/IGOTO for source-slot error probes.
There is no replacement comparison or search implementation. UBSan is enabled
by default. Set CC/CFLAGS to select another host compiler or instrumentation.
ASan/UBSan success-only runs use STRING_CASE_SUCCESS_ONLY=1 (1,696 cases).
The complete 6,872-case error matrix uses UBSan; error() crosses a setjmp boundary.
The fixture disables only LeakSanitizer process inspection, unavailable in this
sandbox; AddressSanitizer bounds checks remain enabled.

The four models vary WORD32/WORD64 independently of IBY2WD=4/8 on a host with
64-bit pointers. Each executes 1,718 cases covering empty tables, singleton
labels, inclusive ranges, ASCII/Rune comparisons, H as an empty source,
0..7-byte operand/table misalignment, invalid counts, missing module/source,
wrapped table/source addresses, numeric case/goto source slots and error order,
default and matched targets, and legacy native
PC cells. A linear oracle checks sorted tables. Rejected operations must retain
the complete REG image; all searches must retain the complete table image.
Module.prog deliberately differs from Modlink.prog to detect stale code bases.

The compiler's Tcasec serializers in limbo/dis.c and limbo/asm.c advance by
IBY2WD for the count, three cells per row and default target. This change keeps
that format. A nonempty table containing native pointers requires cells wide
enough to hold them: a matched NC64 compiler/runtime uses IBY2WD=8. An empty
IBY2WD=4 table remains usable on a 64-bit host. Nonempty 4-byte-cell tables on
that host now fail with exBounds instead of decoding overlapping pointers.
This is explicit rejection of an incompatible layout, not a bytecode migration.

Uncompiled targets are nonnegative instruction indices checked against the live
Module program and instruction count before any PC publication. Legacy compiled
cells remain raw native PCs, preserving their full declared width. Arithmetic
checks do not prove that arbitrary addresses name mapped allocations. Sorted,
disjoint compiler tables remain the input contract; this is not an overlap
normalizer. No new native CASEC emitter or full Inferno/physical ARM64 acceptance
is claimed by these host tests.
