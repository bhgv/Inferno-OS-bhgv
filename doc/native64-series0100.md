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
