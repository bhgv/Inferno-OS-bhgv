# INFERNO-0199 — exact scalar results in local Command calls

2026-11-15. Base86370948 /0198. Local Command helpers now return byte, int,
big and real, composing with no-argument/scalar-argument calls and the optional
Context/ListString prefix. Ordinary exported init remains void. The existing
Inferno interpreter, native frame binder and JITs execute these calls.

## Result values and addresses

The callee writes directly through its FP32 result-address cell. The address
occupies8 bytes; the value occupies1/4/8/8 bytes for byte/int/big/real. Numeric
results use exact naturally aligned, unmapped caller spans outside headers and
the two reserved tail cells. Managed results still require mapped roots.
The constructor recovers each LEA contract from its independently proved callee
class:1/4/8 for scalars,24 for the existing eight-byte reference contract.
No wire annotation supplied by the producer can assert that class.

The reader starts a result class unknown and derives it from stores and nested
forwarding. Equal-sized big/real classes remain distinct, and scalar/reference
classes never merge. A local CALL defines its scalar result only on its normal
successor. Result sources and explicit arithmetic middle operands require
ordinary definite-flow definitions; taking a result address does not read an
uninitialized result. Every normal RET requires an assigned result on every
incoming path. An inherited FP32 address may be forwarded only by a result-bearing
helper with a compatible result class. Existing acyclic body/packet boundaries
and complete-file refusal remain in force.

Computed numeric returns retain ordinary Dis instruction framing, including a
middle operand and indirect result destination. Only the selected local decoder
accepts that framing; the general scalar decoder is unchanged. The proof checks
the opcode's exact numeric source/result kinds, permits only FP/immediate source
roles and requires an explicit FP or signed16 WORD middle for binary returns.
Conversion and byte-immediate returns reuse existing handlers and checked
immutable-byte folding. A successful first scalar lowering that retained a byte
constant in MP is replayed through that folder; general MP DATA is not enabled.
No public type kind, opcode, section, hash or signature format is introduced.

## Independent execution witnesses

The collector observer now distinguishes scalar results from mapped results.
For an active helper, its native result address must exceed4GiB and belong to an
actual ancestor frame. Reference results match a mapped root exactly; numeric
results match an aligned full-width unmapped span outside the header/tail.
Only reference result targets participate in the managed-root omission control.
The observer never treats a scalar value as a heap pointer.

The source matrix has333 provider/client pairs, including33 new cases: all four
scalar classes, nested forwarding, optional managed prefix, branches, loops,
caller objects live across the call, immediate returns, computed int/big/real
returns and numeric conversions. The complete console matrix has1308 main roots
per target plus missing-file, hash, default-policy, entryless-library and tutorial
controls. Host and ARM64 Linux/QEMU runs exercise C/JIT and real collection;
this is not an Android/Bionic GUI claim.

265 independent valid/malformed images on each target cover result addresses in headers, mapped roots,
tails and outside frames; invalid cells/forwarding; undefined sources; missing
return assignments; altered Big/Real operations; arithmetic middle roles and
conflicting numeric results on different branches. Three active proof controls
remove return-definition, immutable slot-kind and result-class-merge guards.
A separate execution control changes a numeric LEA contract to a reference
contract and must fail with bounds before entering the helper. Existing managed
result-map/GC, argument, graph, array/index, String and Context controls remain.

The producer comparison checks666 deterministic files, unchanged previous300
pair/600 files,62 old scalar clients and all classic outputs against0198.
RU/EN tutorials share exactly three identical Limbo blocks and now execute an
int-returning increment helper before the Context guard. The old numeric-result
source refusal is replaced with an unsupported array-of-string result refusal.

## Reproduction and frontier

Use dis64-command.py and dis64-command-boot.py for execution;
dis64-command-scalar-result-proof.py accepts `--control defined`, `kind`, `merge`.
The native role control is `--case scalarresult-int --scalar-result-map-control`.
Use dis64-command-producer.py with `--scalar-result-baseline` set to0198's Limbo.
All accept the existing ARM64 cross-compiler/runner environment where applicable.

General standalone managed parameters, aggregate results, helper imports,
multiple exports, handlers and general ADT DATA remain outside this selected
profile. The actual16-source AWM preflight still refuses the bounded Dis64
profiles. Android client startup and GUI/APK remain the priority, not an achieved
result. No whole-VM sanitizer/concurrent-GC, physical ARM64 or RV64 claim is made.

FlashVM master rechecked85a26cf0a8d3526bfe5d85b4048dc7640a0c6857: two new commits
add VFS publication/export and anchor work, not a Dis64 journal/wire revision.
Incoming0202/0203 stay verbatim. Own journal reports the scalar-result contract;
no DisVM runtime, concurrent ownership model or managed wire-ID agreement is
imported. Vendor pin7f224145 is unchanged.
