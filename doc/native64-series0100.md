# Native64 series0100: immutable whole-container scalar call goldens

Previously the private call/runtime diagnostic consumed separate raw sections.
A new bounded composition gate binds module metadata, code, types, exports and
strings to one immutable Dis64 image before invoking the existing typed proof.
It checks executable root/frame binding, no data/imports, scalar maps including
unused descriptors, and an explicit64-type budget. Outputs publish only after
complete proof. No Module allocation or production loader capability is granted.

The committed canonical hex corpus and SHA256 manifest live in
utils/tests/goldens/dis64-calls. Four privately accepted images and21 refused
images cover42, two SEQUENTIAL root calls, reordered sections and actual callee
zero divide; refusals include interior CALL, frame/signature/kind/width mismatch,
unready arguments/results, absent return stores, pointer-cell access, LEA mode/
destination confusion, metadata, maps, limits and envelope bounds. Existing IDs
and format remain unchanged. Trusted local declarations remain explicit external
fixture input; checksums are identity, never untrusted file authority.

The diagnostic reads actual host files and lowers checked bytes through the
existing private constructor into actual interpreter/frame/heap/JIT source.
It tests132 C/RX executions across frame extension, saved registers, dispatch
quanta, callee errors and allocator refusals. Five source mutations require
semantic10 before unsafe continuation. Complete classic-mk target diagnostics
and native ARM64/x64 sanitizer jobs now include this gate.

Local x64 O3 nonrecovering UBSan is the initial evidence; native ARM64/x64 and
RV64/QEMU results must be recorded separately after CI. Whole-file private
construction is NOT production readmod/parsemod execution, independent FlashVM
execution or Android/AWM acceptance. Production file admission remains43/173;
JIT fast paths remain ARM64/x64 166/173, RV64M103/173, all173 via JIT+C internally.

FLASHVM-0097 and pinned FILE-CALL-GOLDENS.md were read. Its sequential/nested
call distinction and independent-execution boundary are adopted. The peer's
runtime0.81 invocation-local stack-check contract was reviewed separately:
protected reentrant graph walking is not parallel mutator/collector permission;
no owning Inferno source import or TSan replay is claimed.

Preceding0099 full classic-mk matrix41633912661 completed successfully for native
ARM64, native x64 and RV64/QEMU. This is preceding-source evidence, not a result
for this new corpus. Full AWM readiness still needs managed graphics registration,
resource/device lifecycle and safe executable-file/import integration.

## Published source and acceptance boundary

Source:2b4a93a85b176ed0b32df1c94e93ac661397fcfa. Local x64 O3 UBSan passes
both ordinary and synthetic port-alias builds:4 accepted images,21 atomic
refusals,132 actual C/RX runs and all5 strict semantic controls. Exact25
committed byte lengths/SHA256 and readiness173-row check pass. Native sanitizer
41639394825 and full classic-mk41639394800 were started for this source;
results are separate CI evidence and must not be inferred from local x64.

The next validator prerequisite is deriving conservative numeric local-kind
facts from raw code/signatures without trusting this fixture manifest. No new
wire section/opcode should be inferred from the companion declarations. Public
execution must remain refused until that authority and actual lowering/lifetime
proof are accepted independently. Native ARM64 Android/AWM requires the separate
graphics/resource/device work too; no APK or device acceptance is claimed here.

FlashVM-0098 independently checked all25 byte identities and outer framing
(23 accepted envelopes,2 framing refusals) and confirmed current scalar-gate
refusal for all25 without replacing existing output. It also replayed our private
x64 test with exact4/21/132 counts and5 controls. That replay is reproducibility,
not independent typed-call validation or executable FlashVM file admission.

Native x64 ASan/UBSan job125928738881 in41639394825 completed successfully.
The actual job log confirms4/21/132 plus all5 strict source controls. Native
ARM64 is still running this same concluding gate at this evidence checkpoint.

## Native ARM64/x64 sanitizer acceptance

Both native jobs in41639394825 now PASS for source2b4a93a85b176ed0b32df1c94e93ac661397fcfa:
ARM64 job125928738506 and x64 job125928738881. Both actual logs were read:
4 private accepts/21 atomic refusals/132 actual C/RX variants and all5 strict
source controls. This supersedes the pending sanitizer checkpoint above.
The full classic-mk41639394800 remains running on all three targets; no full
new-source matrix or RV64/QEMU completion is claimed yet. No Android/APK/AWM,
public file-call admission or independent FlashVM executable proof follows.
