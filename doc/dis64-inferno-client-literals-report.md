# INFERNO-0189 — owned String literals and content operations

2026-11-15. Base ed811716 /0188. Selected client and Command.init files now carry
ordinary readonly String literals, including Unicode/NUL, and execute String
content comparisons and character indexing. The existing private managed-file
literal convention is reused; no public kind/section/opcode, signature version,
.m function hash or native struct layout is assigned or changed. No FlashVM
runtime/source is imported. This is not a general managed initializer ABI.

## Implementation

The compiler collects only constant String/nil globals and checks every typed
legacy/native data location. It identifies the actual single file path through
ILOAD operands; it does not mistake another literal for the module path. All
String reads and comparison middle operands relocate to the corresponding owned
MP root. The established private convention places trailing Rune32 records after
binding names, roots at MP24+8*i, with at most32 literals,255 Runes per record and
1024 Runes in aggregate. Identifiers/path remain restricted ASCII metadata; data
records preserve all Rune32 bits. Scalar/array DATA and
mutable initializers are still refused.

MP bytes and full map8 cover the three existing prefix roots plus literals.
The zero-literal image keeps its original two descriptors and bytes. A grown MP
uses descriptor1 for its real size and adds independent descriptor2 Context24/E0;
INEW/INEWZ must use2, never the enlarged MP1. Independent raw admission checks
pool/type/data agreement, all maps and bounds, descriptor selection and actual
use of every literal root before construction. The general scalar decoder is
unchanged. The existing private face0186 Command signature/hash4244b354 is kept.

Construction creates native owned String objects before publishing the module,
stores each reference into pinned MP immediately, then copies its contents.
ASCII records may use byte storage; other records use Rune storage. Normal
linkmod clones MP and shares each String reference through ordinary ownership.
No pointer into file data survives; allocation failure destroys all partial roots,
descriptors, imports and exports. Existing module registration/unpin ordering is
preserved. Incremental mk dependencies include the new checked literal header.

Selected lowering/admission now covers IINDC and all six existing String content
comparisons. Index inputs must be initialized Words (or valid middle immediates),
String operands must be mapped/read-only roots, results must be numeric cells,
and branch targets obey existing CFG/island rules. Existing production stringcmp,
branchstring and indc execute; no handler/JIT/collector body changes are made.
Signed Rune32 ordering, nil/empty content equality and bounds/nil errors remain.
Source `""` keeps classic nil-like representation. Serialized zero-Rune records
create owned non-nil empty Strings, a separate constructor-level case.

## Evidence

-22 new literal pairs: ASCII, Cyrillic, supplementary characters, embedded NUL,
 empty/nil, content equality/order, multiple argv Strings, character walking,
 negative/end/nil/empty indexing, Context construction with expanded MP, private
 cons cells, caller-side operations, reload, duplicate constants and limits.
 Actual source-wire decoding independently compares every emitted literal with
 Python expected code points, including NUL; the32-root source case must emit
 MP280.255-Rune individual and1024-Rune aggregate boundaries execute normally.
- All70 Command pairs/6720 scheduler roots pass per x64 and ARM64 Linux/QEMU,
 including six earlier native-seeded inputs. Both caller/provider C/RX, growth,
 quanta1/3/1000, open/admission failure and dontcompile execute with actual gc.c
 epochs between quanta. Native source and cloned MP checks compare each owned
 String with every wire Rune and require corresponding references to be shared.
 Actual characters/content are also checked by Limbo code, not just lengths.
- The independent observer now follows process/frame Modlinks, MP and source MP
 alongside frame/list/record graphs. A control keeps the established module/
 nil/path prefix marked but omits only extra literal roots; it must lose an
 observed String and return semantic exit10 before the guest resumes, on both
 targets. Callee-frame omission detects a private list as before. The bounded
 test arena/observer grow to256 slots; hmsize reads that actual pool geometry.
 This changes test instrumentation, not production allocation/collection policy.
-36 independent raw images check MP/pool/Context agreement, maps, allocator type,
 unreferenced roots, empty/full-bit Rune records, individual/aggregate limits,
 String/index role confusion, immediate modes and branch targets. Four pure
 guard substitutions (unused root, allocator descriptor, aggregate budget and
 class matching) return semantic exit10; malformed files never execute. Host
 ASan/UBSan and ARM64 pass. Prior41 Command,38 field and41 reference image groups
 and all controls pass again, as do previous28/21/64/26 scalar groups/controls.
- Constructor-only host ASan/UBSan covers all70 pairs and every provider read/
 construct/link allocation refusal. Two extra constructor-only wire witnesses
 materialize a non-nil zero-Rune String and [0,80000000,ffffffff] without loss on
 x64 and ARM64, with host ASan/UBSan. Their modified-value code is not executed.
 Whole-VM ASan/concurrent collection remain outside this evidence.
- Rebuilt complete consoles pass256 main Command roots each (64 all-source pairs,
 valid/missing provider in C/RX), hash/default/entryless controls and the updated
 identical RU/EN tutorial with actual Unicode/NUL argv. All214 previous main
 console roots pass again. Nil index raises the existing nil error; a non-nil
 out-of-range index raises the existing array-bounds error.
- Prior62 scalar pairs/5952 roots pass again with actual collection: combined
 132 pairs/12672 roots per architecture.140 Command files are deterministic;
 all62 scalar images match0185,26 Command images match0186,50 match0187 and96
 match0188. All202 compared classic outputs match. Eight well-typed unsupported
 sources refuse without payload, including33 literals,256 Runes and1025 Runes.
 Managed-producer's obsolete extra-literal refusal now tests unsupported mutation.
 Actual AWM preflight remains14 accepted/346 refused bodies, classic unchanged.

New raw/wire/GC gates are integrated in native64-port.py; the regular Command
matrix automatically covers70 pairs. Working examples and exact private bounds:
[EN](dis64-command-init.md), [RU](dis64-command-init.ru.md).

## Remaining boundary and peer state

String mutation, concatenation and slicing, general ADT/array payloads and DATA,
additional imports/exports, constructor expressions inside prepared call setup,
channel services, real Draw/Tk resources and full AWM remain. Linux/QEMU is not
Android/Bionic GUI/APK/device execution. Source and serialized empty Strings have
distinct ownership representations as described above; C extensions must honor
String length/representation and must not use strlen on embedded-NUL values.

Current experiment headed811716 has no new peer post beyond FLASHVM-0202/0203
received in0186. Vendor remains7f2241455734e1142a3254fad9d1ec4f94032ac7. Existing
private literal placement is reused, with an independently checked local Context
descriptor; this does not request a new shared ID or claim general interoperability.

Four additional scalar-library pairs exercise Unicode length, direct MP String indexing, content ordering and embedded NUL through both result binders:384 actual-GC scheduler roots per architecture. Including these,136 pairs/13056 roots pass on each target; all206 compared classic outputs match.
