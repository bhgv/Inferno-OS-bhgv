# INFERNO-0195 — selected int-array allocation, slices and indexing

2026-11-15. Base0824f787 /0194. Selected Command libraries and file clients now
compile and execute zeroed arrays of int, length, owning slices, aliases and
indexed reads/writes. Both byte and int arrays can coexist. This ports another
existing Inferno runtime path; no DisVM runtime is imported.

## Element and address contracts

This Dis64 profile keeps signed32-bit Limbo int values and64-bit references.
The int element descriptor is exactly size4/alignment4 with one zero map byte.
The existing byte descriptor stays size1/alignment1. Optional primitive records
follow Context in canonical byte-then-int order, only if constructors use them.
Maximum type count is five when MP literals require a separate Context record.
An int-only image needs no unused byte descriptor. Both primitive native dtype
maps have np0: their payload contains no managed references. Existing images
retain their exact byte representation.

The proof lattice now distinguishes an unknown managed root, an array root, and
exact byte/int array classes. Length and slices require an array; constructors,
String codecs and indexing require the exact element class. MOVP propagates
compatible class refinements in both directions, while byte/int and non-array
roles conflict. Generic array operations never erase an established element
class. Invalid roots, duplicate/out-of-order/malformed/unused primitive types
and mixed-width consumers refuse before native construction.

The producer recognizes adjacent INDW/MOVW read or write pairs, including local
and immediate indexes/values. It uses0194's existing unmapped native frame-16
scratch pointer cell; scalar MCALL results stay at frame-8. The verifier refuses
edges into the consumer and checks scalar initialization, element class and
exact scratch/consumer roles. Actual runtime scheduling still has an instruction
boundary between the pair, protected by the mapped array owner. Immediate INDW
indexes are numeric native operands, using the same corrected materialization
rule as INDB. JIT emitters and C array handlers are the existing0193 code.

No new public kind, signature hash, opcode, Dis64 section or frame convention is
introduced. Big/real/reference arrays and general array initializer DATA are not
admitted. The selected consumer is MOVW; inline comparisons/compound arithmetic
on an indirect element remain outside the pair whitelist. Read into a local
before comparing or calculating, as the RU/EN tutorial demonstrates.

## Evidence and reproduction

* 210 source pairs with actual collector epochs, interpreter/JIT caller/provider
 combinations, stack growth and96 scheduler roots each on x64 and ARM64/Linux
 QEMU.33 new int-array pairs cover signed limits, neighboring cells, loops,
 aliases, nested backing views, mixed byte/int buffers and error paths.
* 816 main roots in each rebuilt complete x64 and ARM64 emu, plus missing-file,
 hash, default-policy, entryless and RU/EN tutorial controls.
* 164 independent valid/malformed int-array images on both architectures; host
 proof also uses ASan/UBSan. Active element-class, unused-descriptor and descriptor
 alignment omissions fail semantically without executing malformed files.
* The independent pause observer checks `data +4*index` after INDW, including full
 native pointer identity. Owner-mark omission only during that pause and a
 backing-owner omission fail before resumption on both targets.
* 38 scalar-library client pairs include five new int-array-then-MCALL cases.
 The negative-return case exposed and fixed a test-oracle bug: copying4 bytes to
 an unsigned64 accumulator zero-extended a negative int. The oracle now decodes
 through signed WORD before comparison; production result binding is unchanged.
* 420 deterministic selected files, prior177 pair/354 selected images,62 prior
 scalar-client images and all classic outputs are byte-identical to0194.
 RU/EN blocks are identical and compile with the new source-built compiler.

Use dis64-command.py and dis64-command-boot.py for source/full-console execution;
dis64-client-intarrays-proof.py also accepts `--control classes`, `unused` or
`shape`. Run dis64-command-producer.py with `--int-array-baseline` pointing to
0194's compiler. dis64-client-literal-scalar.py covers native scalar return
binding after index use. GC controls use `--case intarray-write --gc-control
index-owner` and `--case intarray-nested --gc-control array-backing`.
The existing ARM64 runner/cross-compiler environment applies.

## AWM and peer boundary

The real AWM preflight still reports all16 ordinary startup modules compiling
classically and refusing the bounded scalar/managed Dis64 profiles. General
module/ADT/call admission, builtin startup dependencies and Android execution
remain separate work. These are Linux/QEMU results, not Android/Bionic GUI/APK
or physical ARM64/RV64 acceptance.

FlashVM master af92efe2f129e15fc1c76a01a5ee84dc063fe592 is one commit ahead of
c9dc3b4c, adding provider watches/system inboxes. The commit changes no peer
journal or array-index contract. Its mirrored upstream journal ends at0172;
we retain the newer local incoming0202/0203 records verbatim.0194/0195 source
lowering and ownership results are relayed through the own Inferno journal;
no new wire-ID agreement, peer import or vendor-pin change is implied.
