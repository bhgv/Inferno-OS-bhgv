# INFERNO-0200 — inferred managed parameters in local Command calls

2026-11-15. Base3b3734d2 /0199. Selected local helpers now accept strings,
lists of strings, byte/int arrays and Context references, mixed with the existing
numeric arguments and optional Context/ListString prefix. The existing native
MOVP/frame/RET and GC paths carry these values; no DisVM runtime is imported.

## Derivation and ownership

A pointer store only establishes an argument's eight-byte framing, not its
pointee class. The reader records managed argument placeholders, proves each
managed-input callee before its caller (including void callees), and derives
parameter requirements from ordinary body operations. String/list length and
operations, array indexing, Context field roles and checked nested call sources
constrain the existing private root lattice. The caller must supply the inferred
class at the exact densely aligned offset in the same prepared frame. Numeric
arguments retain exact kind/width and definite-flow checks. Managed parameter
spans must be mapped; their actual type is not invented by the GC bit.

An input whose class remains completely unknown is refused. Nil locals may
still refine to a known expected class: their initial value is the existing heap
nil. Generic Array is a valid length-only input constraint; ByteArray/IntArray
are required for element accesses. A generic Array result is refused rather than
letting a caller invent its element class. Concrete array returns and existing
managed/scalar forwarding continue to work. No public kind, hash, opcode,
section or signature annotation is added to the file.

Two leading MOVPs keep the existing Context/ListString-prefix interpretation.
Other leading managed pairs, such as two strings, remain ambiguous and refuse.
One leading managed argument works; arbitrary selected managed/numeric mixtures
work after the fixed prefix or a numeric initial argument. The bound remains
eight arguments beyond that optional prefix and eight helpers. This is a bounded
Command profile, not acceptance of every Limbo function or ADT.

String/list/array lengths may now write directly through the checked FP32 result
address, with an int result and the correct source-root requirement. The full
result address remains native64; ordinary scalar/reference result contracts and
all-path assignment checks are unchanged. Returning a computed slice still uses
an explicit local followed by its normal managed result store.

## Execution and refusal evidence

* 368 source pairs, each through96 scheduler roots on x64 and ARM64/Linux-QEMU,
 with real GC and caller/provider C/JIT.35 new cases cover String copy-on-write,
 list heads/tails, nil, shared array mutation/slicing, Context identity, nested
 calls, mixed packed byte/String/list/real arguments, eight mixed arguments after
 the prefix, direct length results, caller/callee objects live across calls and
 helper exceptions.
* 1448 main roots per rebuilt full console, plus hash/default-policy/entryless
 and tutorial controls. RU/EN tutorial blocks remain identical and now pass the
 String label to the existing validate helper after the numeric argument.
* 280 independent valid/malformed parameter images per target. Controls omit
 caller class validation, overwrite inferred callee classes, and omit the new
 void managed-input dependency ordering. Each causes a semantic proof mismatch,
 without executing malformed foreign code. Missing maps, wrong classes, altered
 offsets/frames/opcodes, header/immediate sources, setup entry and result-length
 stores are covered. Host proof and368-pair construction/rollback run ASan/UBSan.
* A new collector control omits only inferred managed parameter roots, preserving
 other locals. The helper replaces its String parameter with a fresh concatenated
 value and enters another helper. Removing that parameter root loses the sole
 owner and is detected before resuming the retired graph. The normal control-free
 C/JIT runs retain and finally retire the complete graph. Previous managed-result,
 frame/map, scalar, graph, array/index, String and Context gates remain exercised.
* 736 deterministic files. Prior333 pair/666 selected files,62 old scalar clients
 and every classic output match0199. Sixteen well-typed source refusals include
 unresolved input classes, an ambiguous leading managed pair, an unresolved array
 result, aggregate results, recursion/imports and existing limits.

Reproduce with dis64-command.py, dis64-command-boot.py and
`dis64-command-producer.py --managed-argument-baseline <0199-limbo>`.
The new raw suite is dis64-command-managed-argument-proof.py, with
`--control classes`, `inference`, `order`. The root ownership control is
`dis64-command.py --case managedarg-rebound --gc-control local-argument`.
Use the existing ARM64 cross-compiler/runner environment. No dependency is added.

## Actual AWM frontier and peer receipt

AWM preflight now checks all four flags: -64, -64l, -64b and -64c. All16 startup
sources still compile normally and refuse all four bounded profiles, with no
nonempty partial output or sanitizer failure accepted as refusal. Current -64b
AWM diagnostics identify module data/imports/handlers outside the call-file
profile; -64c reports the importing Command shape boundary. These are general
module serialization/linkage barriers, not a missing meaning for Limbo load.
Builtin/file import hashes and generated String argument/result layouts are
still checked from the actual sources. No AWM execution or Android GUI/APK is
claimed. Android ARM64 remains the priority; Linux/QEMU results do not establish
Bionic/device startup, physical ARM64/RV64, concurrent GC or whole-VM sanitization.

FlashVM master remains85a26cf0a8d3526bfe5d85b4048dc7640a0c6857 at receipt.
Incoming0202/0203 and our journal prefix stay verbatim. The own journal reports
the inferred parameter/GC contract; no runtime/concurrent ownership migration,
managed wire-ID agreement or vendor-pin7f224145 change is made.
