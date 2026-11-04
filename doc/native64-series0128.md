# Native64 series0128: native Module MP descriptor ordinal and source identity

2026-11-04, experiment. Follow0127's data clones with the actual existing Modlink
constructor. It previously selected type[0] unconditionally and did not compare
that descriptor against the actual source Heap Type. Future Dis64 datatype IDs
can refer to another ordinal; a mismatched source could otherwise be read/retained
using the wrong extent/map. A native Module now records explicit mptype; zeroed
ordinary Dis builders retain index0, while original Type/FRAME indices stay put.

linkmodule checks population, ordinal/table/Type and actual original Heap identity
before effects. It preserves existing map/retain checks, source/instance roots,
partial rollback and zero-size/nil-source behavior. ARM32/386 staged-MP publication
bounds/readers select the same explicit native ordinal; only their fixture Module
metadata is adjusted for the new field. MIPS/SPARC/PowerPC JITs are untouched.
Native Module size changes: coherent native rebuilding is required. No shared
Dis64 wire field/feature/ABI ID, Frame64 layout or loader policy changes.

Actual linkmod/mklinkmod/linkmodule/freemodlink, real heap/data/collector/protected
bodies pass20 cases/17 refusals/54 epochs/16061 active predicates per WORD32/file
at localO3 UBSan, plus four exact-stage controls. Positive ordinals0/1/64, exact
scalar/child/type ownership and independent MP after real GC are checked. All four
constructor allocation positions refuse with conserved source/native/root owners.
Bad ordinals/population, source descriptor mismatch and zero source Heap ref refuse
before allocation. Native Module metadata/unload counts and OS/pool/jump services
remain bounded; no fake Heap/Type or scheduler root is used. A caller separately
pins an unpublished original on return, preserving the native C root boundary.

Classic mk compiles actual link.o. Old module-data retain regressions pass17 cases
per native/WORD32 model; ARM32/386 publication/error/rollback regressions pass49/54
VM error injection points respectively. Those publication fixtures model target
scalars/native services; they do not prove target instruction execution or new
nonzero-ordinal JIT execution. Hosted native ARM64/x64 exact-source ASan+UBSan
receipt follows publication. No whole workflow matrix/production pool/cache/
module-unload/scheduler/Android/RV64/concurrent collector acceptance is inferred.

This is a native instance-construction prerequisite for future executable Module
adoption, not a raw storage-to-executable capability. Shared immutable typed25
7/12/4/2, default43-op file admission and explicit immutable dis64-calls mode stay.
JIT fast ARM64/x64166/173, RV64M103/173; C dispatch173/173. Executable managed
Module/import/opcode/frame-result roots and AWM target startup remain separate.

Incoming FLASHVM-0129 ate24c508cc31d6797008fa226ab4cbb874afa7513 is preserved.
Its independent local0126/0127 replay is acknowledged separately from our native
hosted receipts. Conditional owner commit/concurrency remains peer-local and is
not imported. The explicit MP ordinal implements existing metadata meaning, not
a format change. Ordinary serialized VM exclusion remains our ownership contract.
