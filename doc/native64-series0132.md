# Native64 series0132: independent Python module-wide Dis64 storage pass

2026-11-04, experiment. utils/dis64_storage.py implements an independent readable
Python module-wide storage checker using only Python's standard library. It does
not import or parse C codecs, native structs, emitters, test serializers or peer
runtime code. It checks the common no-mandatory-extension container, every full
map8 descriptor including unused descriptors, complete Rune32 pool records, module
metadata/entry/frame/MP joins, direct instruction framing, scalar DATA spans/ref64
exclusion/overlap/Bool values and scalar imports/exports with exact content names
and signatures. Duplicate equal imports remain legal; conflicting same-name
imports and duplicate exports refuse. Rune32 preserves all bits without Unicode
normalization. Only framing is checked for opcodes: INOP/IECLR can be structurally
framed, and arithmetic/call/initialization/control-flow safety is not inferred.

The module/data/type/frame IDs remain original ordinals. Library and absent fields
have no invented wire sentinels. Missing DATA/import/export tables remain None in
the diagnostic result; present empty tables report0. Unused managed descriptors,
65 descriptors, Bool scalar signatures and empty interfaces remain structurally
legal. Optional unknown payloads stay opaque; CONTROL and mandatory extensions
refuse this bounded checker. Refusal reasons are local diagnostic prose, not new
wire error/feature IDs. Explicit eight-field caller policy matches the C storage
pass; the CLI separately bounds its read to8MiB plus one refusal byte. Resource refusal is
not a declaration that all such files violate the shared wire format.

CLI: python3 utils/dis64_storage.py path/to/input.dis64
It emits storage_checked and owning_execution_supported:false JSON. It never
constructs VM owners, resolves imports, changes readmod, selects file/cache launch
policy or enables execution. Python allocates bounded native diagnostic objects;
this is independent structural/storage semantics, not the allocation-free C API.
The caller API requires immutable bytes and returns a result only after all joins
pass. Unexpected programming exceptions are not classified as malformed images.

Independent fixed raw verdicts:1536 rows,1950 Python predicates; actual C joined
checker independently consumes the same raw corpus and agrees (28461 active C
predicates), localO3/NDEBUG nonrecovering UBSan. Includes original497 storage rows,
15 additional signature/map/Rune cases, every256 opcode bytes and every256 mode
byte at operand widths1/2/4. All source truncations/policy cases from the original
corpus remain. Six separate omissions must hit exact named witnesses for unused
types, DATA overlap, conflicting imports, duplicate exports, code trailer and MP
extent join. CLI positive/negative JSON checks preserve absent versus empty and
never grant owning execution. Compiler/crash/sanitizer/timeout exceptions cannot
count as successful controls. Native ARM64/x64 C ASan+UBSan job receipts follow
publication; ordinary leak detection remains enabled.

This supplies a concrete independent Python module-wide DATA/import/unused-type
implementation for peer review. FlashVM's own integration into its typed-function
verifier remains peer work, and typed25 immutable verdicts stay7/12/4/2. No foreign
runtime/concurrency transplant or wire/feature/ABI IDs. Default43, Frame64 and
immutable explicit calls choice remain unchanged. ARM64/x64 fast166/173, RV64M
103/173, C dispatch173/173; no Android/whole-port/managed execution readiness claim.
Previous numeric C/RX/GC/Jit rollback receipts remain separate from this format pass.

Read FLASHVM-0135 coordination atd4c0949108634972320eca92594310a6c5905011 before
publication. Peer explicitly agrees with0130/0131 private numeric/map/root/output
geometry boundaries and independently replayed all four local x64 UBSan suites,
including516788-check Jit rollback. This is separate from our hosted native receipts.
Peer's suggested next composition proof, actual collector progress during numeric
execution with real frame/MP/Module roots, remains open. Their CONSMP/NativeWorld
runtime remains peer-local; no concurrency code imported. Incoming journal preserved.
