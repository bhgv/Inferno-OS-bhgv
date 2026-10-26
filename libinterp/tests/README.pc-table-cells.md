PC table cells and native address preflight
==========================================

The Limbo serializers in dis.c advance by IBY2WD, independently of native
pointer size and interpreter WORD size. C and AMD64/ARM64 table readers now
follow those offsets. Integer counts/targets/bounds remain signed 32-bit
fields; big bounds remain signed 64-bit fields. Padding is not a value.

| Table | Header | Row or entry stride | Target within row |
| --- | --- | --- | --- |
| ICASE | IBY2WD | 3*IBY2WD | 2*IBY2WD |
| ICASEL | 2*IBY2WD | 2*IBY2LG+2*IBY2WD | 2*IBY2LG |
| IGOTO | count at entries-IBY2WD | IBY2WD | selected cell |

IBY2WD=4 retains the existing 12/24/4-byte row/entry layout. IBY2WD=8 uses
24/32/8 bytes. Compiler Tcasel sizing previously reserved six cells per row,
which exceeds the actual serializer's two big bounds plus two padding/target
cells when cells are eight bytes. The size calculation now matches emission;
its checked count multiplication and final padding remain intact.

This supports matched compiler/runtime cell widths, not automatic conversion
between bytecode layouts. It does not introduce a new native opcode or finish
the NC64 VM ABI. Numeric legacy compiled tables retain their existing raw
32-bit native-PC handling. CASEC uses its separate declared-cell reader.

Native ICASE, ICASEL, IGOTO and MOVPC validate source slots and the complete
needed link/module field spans before loads. MOVPC validates its destination
span before storing. Zero, H and arithmetic wrap decline without changing REG,
operand/table storage or FP state. The C fallbacks now reject wrapping link/
module headers, and MOVPC supplies ordinary VM errors for bad operand slots.
These arithmetic guards do not establish that arbitrary addresses are mapped.
Valid overlapping source/output slots retain capture-before-store behavior.

Validation
----------

* `python3 libinterp/tests/pc-tables-native.py`: 46,236 C/actual AMD64 RX cases,
  varying WORD32/WORD64 and cell4/cell8 independently, every byte alignment,
  selected/default targets, stale Modlink programs, padding and errors.
* `PC_TABLE_SUCCESS_ONLY=1` with ASan/UBSan/NDEBUG: 25,024 successful C/RX cases.
  Full nonlocal VM error tests use UBSan; unavailable LeakSanitizer process
  inspection is disabled without disabling address checks.
* `python3 -O libinterp/tests/pc-tables-jit.py`: 50,624 AMD64/ARM64 instruction
  cases with compact and distant fields, hostile pointers, signed counts,
  ranges, complete data images, preserved callee registers/stack and FP state.
* `python3 limbo/tests/case-cell-layouts.py`: executes actual compiler serializer
  arms for 72 family/count/wildcard layouts and checks their write offsets.
* `python3 limbo/tests/layout-bounds.py`: 4,110 type/frame/table limit checks in
  cell4/cell8 models, including the production Tcasel size calculation.

The existing leaf instruction fixture now mirrors all live LeafLayout fields
and uses the current 128-unit output capacity. Its 217 former Python asserts
are explicit runtime checks, preserving them under python -O. Insufficient
output capacity still checks canaries against the actual fixture buffer length.
The targeted case/goto/MOVPC sections remain independent regression oracles.

The host checkpoint includes both new host drivers and compiler layout bounds;
--jit additionally includes the new two-architecture instruction matrix.
A complete Inferno build/boot and execution on physical ARM64 remain separate.
