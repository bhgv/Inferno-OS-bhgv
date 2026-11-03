# Native64 series0091: Loader ownership and checked native PC snapshots

## Production changes

loadermodinit registered the module first, then assigned three unchecked dtype
results to globals. It now stages instruction/type/export snapshot descriptors,
checks registration and publishes only after complete success. Failure releases
all staged descriptors while retaining the original registration diagnostic.
Complete initialization is idempotent; partial Loader state refuses unchanged.
Sys initialization remains independently owned and is never undone by Loader
cleanup. The caller holds VM initialization exclusion.

Branch and export snapshots formerly subtracted native pointers without checking
that the target belonged to the declared code image. Shared loaderpc now checks
nil/H, positive instruction count, alignment, multiplication/span overflow and
target range/stride using uintptr before returning a Dis index. No target is
dereferenced and no snapshot field is changed on refusal. Export nil-PC/no-code
sentinels retain their existing -1 behavior. Relocated native-PC modules still
refuse snapshots. Geometry checks do not authenticate arbitrary host memory;
the caller still owns a valid decoded Module/image.

## Validation

New actual-body initializer fixture uses real native records, dtype/freetype and
VM error protection. Sys initialization and builtin registration are injected
boundaries, not proof of complete Sys/Loader module activation. Per WORD32/64:
15 allocation/refusal/retry/partial cases, exact three maps, idempotence and
Sys error propagation. Three mutations require semantic status10. Fatal negative
predicates clean only tracked fixture allocations after failure, so intentional
control leaks do not mask the predicate with LSan. Positive ownership balances
without this harness cleanup.

New native target fixture runs2800 opcode/shape cases per WORD32/64 (all175
enum IDs including reserved ones):99 valid branch-index conversions and429
bounds refusals, with real pointers above4GiB. Width/bounds mutations require
semantic status10 before any target execution. Module, instruction and all
non-destination snapshot bytes remain unchanged.

Existing public Loader snapshot suite now covers18 invalid branch/export PCs
at early and late array elements: temporary arrays/strings are fully retired,
source metadata and descriptor refs remain unchanged. All304 public cases pass
per native/WORD32 model. Generated checks remain active with NDEBUG; the driver
uses the selected target headers/runner and isolated allocator boundary. Its
legacy LSan opt-out remains; tracked ownership and ASan bounds are separate
from LSan certification. The initializer/branch fixtures have no such opt-out.

Local x64 O3 nonrecovering UBSan passes these gates and all five strict controls.
Full Loader consumer syntax passes both word models. Existing loader-runtime
protected-error/MP restoration checks also pass. Classic-mk ARM64/x64/RV64 and
focused native ARM64/x64 address jobs gain the gates; fresh target results are
pending publication. Prior0090 full matrix41584398790 completed successfully
on all three targets, alongside its focused address/Bionic checks.

## Coordination and remaining work

Read FLASHVM0080 OWNER-SLOTS.md at8474d1723aa2aecf54a19f3b2f1b9b109b342c97.
Protected source retain before unlock, moving the prior owner out before later
retirement and joined slot lifetime are compatible native extension constraints.
This is documentary direction review, not slot implementation replay/import.
Managed roots, publication and whole C/JIT/GC concurrency remain independent.

No shared Dis64 ID, native pointer serialization, JIT emitter or new file-call
activation changes. Full managed module/file ABI and AWM remain work. Native
path availability stays ARM64/x64 166/173 and RV64I/Zmmul/M 87/90/103, with
native+C173/173. Production file admission stays43/173.


## Completed focused native and Bionic evidence

Native ARM64/x64 address run41587971598 passes at source83b5fb23092f31b7aa2f15d73181fa6c7ebb983a:
initializer15 cases per WORD model/three strict controls;2800 PC cases per
model/two strict controls; public304 cases per model including18 cleanup
refusals. Downloaded both archives and independently checked ZIP digests plus
all positive/control/public cleanup markers. Snapshot LSan scope stays as above.

| Address artifact | ZIP SHA256 |
|---|---|
| ARM64 13209410281 | 5035ba22fbbc2b9f41c4a9e97bd1a7b9cbc686c394233b63c2e7cd97875e4791 |
| x64 13208752381 | fd88a6b183760ce38be8b5bd94676b6a6a13b65680b1ee2f4503ddba015a0e99 |

Android run41588361296 passes at launcher271f22d0db79f47c6a95b53f20eb0be8ce021ffe
with exact core83b5fb23. NDK compiles full Loader; Bionic/QEMU executes all new
initializer/PC/public snapshot cases and controls, then previous Draw/binding/
module/Sys and whole C/JIT process gates through real worker retirement.
Sys/registration boundaries in the initializer fixture remain injected, not
proof of complete Loader import or full GUI activation.

[Verified APK pair/evidence](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41588361296/artifacts/13209341557):
ZIP SHA256db73d14a0e1fa155c4ecdf64cd3b178de465a432c57c5884678ae46ca7b1d2fe.
Independently checked both APK hashes, exact core.txt, no DEX, ELF64/AArch64,
16KiB LOAD alignment and0091 package identities. APKs are4621134 bytes each;
probe image stays9138ae33fb041adb8b0b689ad1e5c1420fdb5d86a33f0c4dad6c6e96b05c18e0.

| Mode | APK SHA256 |
|---|---|
| C | 122b46a4ad693a8e2bd354e8bf18158851cd2fca4292cff480883cd9cee38416 |
| JIT | aae361c08107a3ea08023d6c5ecdcc985f4b6eb129f226c112095889e31408d8 |

Full new Linux classic-mk run41587971674 remains pending at this report. Existing
0090 full target success is separate. Physical-phone AWM and general managed
file-call/import admission are not inferred.

FLASHVM0082 independently replays the new initializer/PC fixtures on x64 UBSan.
Read TYPE-COUNTS.md at780b2b8484b30baeec84f529cac3c2ae97ea8c70: signed Type
counts, unique-LAST/no-post-drop-read, static baseline and preflight-not-reservation
remain explicit. This is documentary review, not adapter import or local replay.
Existing exclusion remains required while other C/JIT sites are plain.
