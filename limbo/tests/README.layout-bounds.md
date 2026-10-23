# Checked compiler storage extents

Run `python3 limbo/tests/layout-bounds.py` from the repository root.

The production sizetype, sizeids, idoffsets, align and checked size helpers are
compiled against actual compiler Type/Decl/Szal definitions under strict UBSan.
Two logical word models (4 and 8 bytes) each run 2,055 cases. Two thousand small
declaration lists use independent arithmetic offsets; reused local slots retain
their assigned location. Table counts include the largest representable count
for each case/alt/goto format, without allocating those large tables.

addsize bounds nonnegative extents to the signed 31-bit size range before
addition or narrowing to Szal.size, which is an int even on an LP64 host.
Table counts are checked before native-int multiplication. Interface-name
lengths and frame/field offsets use the same checked addition. align computes
padding in constant time and checks it before addition, instead of incrementing
through a potentially huge or overflowing span. Existing valid layout, grouped
alignment and local-slot reuse conventions are retained.

Fourteen rejected layout cases per model cover excessive aligned extents,
negative sizes/offsets, invalid alignments, overflowing interfaces and excessive
or negative case counts. Children must exit through the compiler fatal path;
normal return and sanitizer termination both fail the fixture. LAYOUT_TYPES_SOURCE
and LAYOUT_DECLS_SOURCE select sources; LAYOUT_CASE selects ordinary, tables or
failure. The original type helpers independently fail the range-rejection test.

The driver runs on an LP64 host with native pointers and substitutes only the
logical IBY2WD constant. It does not establish a native 32-bit long build or an
activated 64-bit Dis ABI. The existing narrower Dis operand encoding remains
a separate output constraint. This change defines no new bytecode format and
does not generate a new JIT backend or boot Inferno.
