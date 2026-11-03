# Native64 series0102: owning raw-byte scalar-call Module construction

The preceding raw-image proof still used a trusted test-only Module builder.
prepare64calls in libinterp/load64.c now performs real unpublished construction:
open one bounded immutable image, check native Frame64/WORD32/binary64 geometry,
check an ASCII display name, allocate one bounded native analysis workspace,
derive local types from raw code/signatures and complete the existing semantic
proof. Only then allocate VM Module/Type/Inst owners. The explicit experimental
constructor bound is256 instructions; it is not a new wire-format maximum.

The full original Type table/order is retained rather than normalizing frame
indices. Indirect destinations are range-checked before narrowing into native
ushort decoder fields. Every native instruction is copied/lowered into owned
storage; branch/CALL targets point into the owned program. Names/path are copied,
MP is H, entry/frame identities come from the checked source. No source buffer,
signature, inferred declaration or temporary proof workspace is retained by the
returned Module. The proved result-address cstep identity is selected only for
this unpublished construction; ordinary WORD32 LEA remains unchanged.

Construction is protected by actual kprotect. Partial modules retain their
cleanup owner across nil/error/compile failure; actual freemod handles descriptor,
program/name/path and JIT owners. Temporary analysis storage is released on both
success and failure. Refused semantic inputs may allocate one bounded native
workspace but allocate NO VM owners. Structural/model/name refusals precede that
workspace. Runtime model checks are factored with the existing scalar constructor.
Public dis64parse/readmod still call only prepare64pilot and its unchanged43/173
admission. No cache/fs identity publication, file CALL permission, external export
linking, imports/managed/Bools or independent FlashVM acceptance is introduced.

Tests execute actual owning load64/heap/frame/interpreter/JIT/cleanup bodies with
explicit bounded native transports. Original39 peer files plus an immutable new
Type-ordinal permutation give10 accepts/30 atomic proof refusals and348 actual
C/RX/extent/quanta/saved/error runs. Each execution overwrites its complete source
buffer AFTER construction; actual Module execution and unload still pass. Twenty
C/RX constructor successes and60 refusals are separately checked; all170 tracked
constructor allocation refusals leave no live native allocation or VM error frame
and never publish into the module cache. This ledger does not claim a global
leak scan or separately injected OS RX-map failure. Invalid API pointers, model/
mode/overflow shapes refuse before allocation. Four actual source controls omit
workspace cleanup, partial-module cleanup, cstep selection or indirect lowering.
They require semantic10 before unsafe execution; private test ledger cleanup only
keeps the failed-control process sanitizer-clean and is not production rollback.

A new canonical frame-order image in utils/tests/goldens/dis64-construct keeps
root frame1 and leaf frame0 (sizes112/88) and returns42. SHA256/length are pinned;
original25+14 goldens are unchanged. Scripts default to exact verification and
require explicit --write-goldens for a reviewed revision. No new IDs/layout.

Local x64 O3 nonrecovering UBSan with port aliases passes the complete constructor
matrix and four controls. Existing scalar constructor regression passes744 whole
images/1667800 predicates plus unchanged legacy WORD64 rejection before allocation.
Native ARM64/x64 ASan jobs and complete classic-mk include the new constructor
matrix; report their actual target outcomes separately. JIT native/C populations
and production file admission remain unchanged. Android/APK/AWM/device lifecycle
and ordinary file-loader activation remain separate work.
