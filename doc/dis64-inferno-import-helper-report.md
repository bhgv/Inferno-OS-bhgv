# INFERNO-0201 — importing clients composed with local helpers

2026-11-16. Base79e77a38 /0200. The selected -64c importing executable now
contains up to eight private helpers alongside its root LOAD/MCALL body. It can
compute before/after imports, branch and loop around them, and use the existing
managed/numeric local parameter/result contracts. Native Inferno frame, GC, call
and JIT paths are reused. This does not import DisVM or enable general module MP.

## Code, descriptors and body permissions

The writer lowers every helper, checks the supported kinds and complete source
span, and plans shared literals/primitive descriptors across the whole client.
Private frames follow the existing MP/Context/primitive descriptors. Root module
handles still use the checked MP0 slot and the copied, owned import table. Helper
String literals and array/Context constructors receive the correct shared MP
addresses and descriptor IDs. No public kind, opcode, signature or section is
introduced. An executable must have exactly its one public init export; other
exports are refused rather than silently dropped.

Imported scalar results insert a copy after MCALL. The original-to-serialized
PC map therefore covers root instructions and all appended helpers. Root/helper
branches and CALL targets use that complete map; FRAME operands use the matching
new descriptor IDs. Local binders retain their caller destinations, while import
binders still use the reserved eight-byte result tail. Private helper imports,
handlers, extra module paths and persistent mutable global data remain refused.

The independent reader discovers the full local graph and then proves the root
as an importing executable and helpers as entryless bodies. The root terminates
with IEXIT; helpers with RET. Only the root permits LOAD/MCALL, retaining definite
load-before-import facts. Local call packets are atomic CFG nodes just like
imported packets. Branches cannot enter setup or cross body boundaries. Local
result definitions do not manufacture a LOAD fact. Existing cycle, argument
class, scalar initialization, return-assignment and ownership checks compose.

The constructor enables typed LEA binding whenever helpers exist, even when the
root frame has no mapped roots. It derives local widths/reference flags from
callee proofs and preserves the eight-byte import-result binding. Numeric import
copying therefore composes with byte/int/big/real and managed local results.
The instruction source buffer is still destroyed after construction in execution
witnesses; helper code/maps/literals must remain owned native data.

## Evidence

* 386 Command provider/client source pairs and11 numeric-import helper pairs per
 x64 and ARM64/Linux-QEMU, actual GC with96 caller/provider C/JIT, growth, quantum
 and namespace modes.18 new Command clients exercise helpers before/after LOAD,
 return classes, nested calls, branches/loops/reload, errors, fresh arrays/Context
 and Unicode literals appearing only in helper bodies. Eleven scalar clients check
 inserted result-copy PC shifts, zero-root caller frames, mixed local/imported
 results and managed input held solely in helper-frame maps.
* Rebuilt full consoles:1520 main Command roots plus44 scalar-helper roots on each
 architecture, with missing-file, wrong-result, policy/hash/entryless controls.
 Matching RU/EN tutorials now execute countargs in the importing Client itself.
* 205 valid/malformed import-helper images each, also host ASan/UBSan. Root/helper
 terminators, entries, frames, arguments, binders, unused descriptors, cross-body/
 setup branches, helper loads and missing root LOAD refuse independently. Active
 controls remove imported-call load requirements or incorrectly prove the root
 as a library, producing semantic matcher mismatches before foreign execution.
* An execution control removes only the typed import-result binder contract in
 a helper-bearing client. It must report bounds before imported function entry,
 rather than crashing. A separate parameter-only GC omission loses a freshly
 rebound String while a client helper is suspended. The test observer now retains
 both client and provider helper types across LOAD, updates reused type identities
 and resets observations on complete process retirement; it does not change GC.
* Host ASan/UBSan construction/rollback covers the Command matrix, helper-only
 constructors/literals, and focused zero-root numeric importing clients. Existing
 managed/scalar parameter/result, array/index, String/Context, graph and namespace
 checks remain. No whole-VM sanitizer or concurrent-collector claim is made.
* 772 deterministic Command files plus11 deterministic scalar-helper callers.
 Prior368 pair/736 selected files,62 prior scalar clients and all classic outputs
 match0200. Sixteen existing library and four new client source refusals include
 extra public exports, imports inside a helper, mutable MP globals and recursion.

## Reproduction and frontier

Run dis64-command.py and dis64-command-boot.py as before. The new cases have the
importhelper- prefix. dis64-scalar-client.py and dis64-scalar-client-boot.py accept
--local; the former requires actual GC in that mode. Its
`--local --case import-local-before --local-contract-control` is the execution
omission. dis64-import-helper-proof.py accepts --control load and --control root.
The collector control is `--case importhelper-rebound --gc-control local-argument`.
The producer accepts --import-helper-baseline pointing to0200's compiler. Existing
ARM64 compiler/runner environment applies; no dependency is added.

This connects existing private helpers and file imports; it is not general AWM
module serialization. Actual16-source AWM preflight still compiles classically
and refuses -64/-64l/-64b/-64c. General data/import/export shapes and handlers,
then Android/Bionic GUI/device/APK startup, remain open. Linux/QEMU evidence does
not establish physical ARM64, RV64, concurrent GC or a working Android client.

FlashVM master rechecked4a40ada1dfcf5125a6e57766dfd937ddaa575ee8: its new commit
adds VFS capability publication/GC integration, with no journal or Dis64 wire
revision. Incoming0202/0203 remain verbatim. Own journal reports this composition;
no runtime/concurrent ownership import, managed wire-ID agreement or vendor-pin
7f224145 change is made.
