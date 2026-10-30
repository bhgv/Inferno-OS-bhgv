# Private generated native64 tables and transactional binding

`LIMBO_NATIVE64_TABLES` requests callback-free module specification arrays from
Limbo's explicit `-H` declaration producer. It also enables the individual entry
records and map storage. A table contains each non-variadic function's name,
existing legacy signature hash, fixed-frame extent and complete map8. Its C
function field is zero. Reading that metadata therefore does not require linking
every builtin implementation. Individually selected entry records retain actual
C function pointers and can be linked with their owning implementation.

This is an opt-in consistency mechanism for generated C. It does not feed the
ordinary Runtab/builtinmod path, authenticate a wire module, or introduce a
structural Dis64 call signature. The installed compiler/VM defaults are unchanged.

## Matching and publication

Include `native64-bind.h` after an explicit generated header with records enabled.
After the generated geometry predicate succeeds, `native64bind` matches a selected
list of trusted implementation records to the complete trusted specification
array. Results borrow the implementation records, in the requested order.

The checker validates both tables before writing any output slot. It rejects:

- empty/overlong names, duplicate names, missing names or missing C callbacks;
- callbacks in specification-only records;
- mismatched legacy hashes, frame sizes or map extents/contents;
- incomplete map8, marked Frame64 header/result/temp slots, and padding bits;
- invalid frame sizes, table/resource limits, missing buffers or insufficient output capacity.

Every specification is validated, including unselected entries. Empty logical
tables are allowed. Names/maps/records must be readable and immutable throughout
the operation and borrowed result lifetime. Output storage must be disjoint from
input tables. No allocation, result ownership transfer, function invocation or
runtime registration is performed by the checker. Limits of4096 records,255 name
bytes and1MiB frames are private implementation bounds, not wire-format ranges.

The legacy hash is explicitly a consistency check. Matching it does not establish
structural type equality or reference-result ownership. A trusted implementation
list supplies the C callback identity. The helper does not validate arbitrary
machine addresses or accept untrusted callback pointers.

## Qualified methods and full maps

Generated names now preserve the ADT qualifier for methods: A.f and B.f remain
distinct module-relative symbols. Individual records previously used only the
bare f spelling. A new real-Limbo module with both methods generates and compiles
both records and table; they bind simultaneously. This fixes a metadata namespace
collision without changing ordinary Limbo signatures or bytecode.

The existing150-reference fixture binds a1264-byte frame with a20-byte map; every
byte is compared. A supplied implementation claiming only16 bytes is rejected,
with output unchanged. No adapter to the old fixed map[16] Runtab has been added.
This checks explicit malformed binding metadata, not a nonexistent adapter.
An all-variadic module emits ISO C storage for a zero-count logical table and no
fixed entry. Its empty binding succeeds without creating a callable function.

## Actual consumer evidence

`native64-layout.py` builds fresh actual Limbo and runs the complete Math driver.
Math's complete owning translation unit is compiled with the explicit header;
selected four C implementations are linked with function/data-section collection.
The complete generated Math specification table binds those four actual entry
records, and192 calls/model run through the resulting bound pointers. Both
WORD32 and WORD32+file are exercised. All68 implementations are not invoked.
The experimental Math initializer still refuses legacy builtin registration.

The entry fixture adds24 explicit refusal cases, including a mismatch after the
first valid selection; sentinel output slots remain unchanged. It also checks
successful empty and full20-byte bindings. The method and all-variadic fixtures
compile and execute real generated declarations. Geometry/maps, deterministic
output, legacy ABI regression checks and early unsupported-shape rejection remain
part of the owning suite.

Reference arguments/results, caller/result/temporary roots, forced collector
checkpoints during ownership transfer, unwind, shared structural signatures and
runtime registration remain separate gates. The43-op np0 scalar file profile is
unchanged. This is not full new-format execution or physical ARM64/RV64 testing.
