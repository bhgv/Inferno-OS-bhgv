# Native64 series0085: compatible static translation binding

The classic Dis-to-C attachment function could overwrite a pre-existing cstep,
and did not exclude a Module with a published leaf-JIT cache. That permitted
silent execution-mode replacement/composition despite0084's checked entry ABI.

`disc_tryattach` now returns an explicit success/refusal result. It requires a
complete valid source span, exact registered source bytes/instruction count,
ordinary PC mode, enabled translation and no JIT cache. An existing callback
must be the selected callback for that exact registered image; a different/H
callback is preserved and refused. Idempotent matching attachment succeeds.
Refusal changes no Module or registry field; success publishes only cstep.
`disc_attach` keeps its existing void ABI and delegates to the same implementation.

Static registration rejects nil/H/misaligned/wrapping descriptor storage,
invalid/empty/oversized/wrapping image spans and nil/H callbacks before publishing
the borrowed record. Registered records/images remain native-host-owned and
immutable, except the registry's next link. This is not an arbitrary-pointer
memory validator or a native-code sandbox.

## Ownership and format boundary

The owning caller must hold the VM lock and bind before publication. The API
does not detect global quiescence or acquire a heap/context lease. Module.ref==1
is not such a proof: mklinkmod can consume an existing reference into an active
instance without incrementing it itself. No run-time mode-switch API is added.

The caller still proves that the Module was decoded from the supplied image.
Byte identity against registered trusted C images is not a new Dis64 container/
declaration proof. The new-format scalar loader does not call this attachment
path. File CALL/LEA8 admission remains closed; no shared format ID/profile, native
layout, atomic RC, object lock, worker or parallel heap mode is added.

## Owning test matrix

`native64-disc-binding.py/.c` includes actual `libinterp/disc.c` with actual native
headers. For each WORD model,1,536 independent state/image/API-wrapper cases
cover nil/matching/foreign/H callbacks, JIT cache presence, native-PC mode,
global disable, DONTCOMPILE, MUSTCOMPILE, wrong instruction counts, equal bytes
at a different address and mismatched bytes. Twelve accepts invoke only the
explicitly selected callback AFTER checking attachment state; attachment itself
must invoke none. Every Module byte and registry link is checked against the
expected unchanged/one-field outcome.

Twelve invalid registration cases and28 invalid attachment/API-wrapper probes
exercise guarded sentinels, alignment/span wrap, lengths, descriptor and mode
failures. These are bounded native-argument probes, not forged arbitrary memory
or a file parser proof. The fixture resets its private registry only while its
borrowed records remain alive; there is no new public unregister API.

Three controls independently remove callback compatibility, the no-JIT rule or
exact-byte matching. Each must reach semantic status10 and the expected predicate;
compiler failures, signals, sanitizer failures and timeouts are not detection.
The classic-mk native port driver runs both models and all three controls on
ARM64/x64/RV64. Local optimized nonrecovering UBSan checks pass; full target
validation follows the source commit.
Local additional ASan attempts are not counted: LeakSanitizer terminates because
this execution environment cannot access its /proc task data, including a retry
with leak detection requested off. UBSan results remain separate. No new local
ASan/LSan or whole-runtime sanitizer certification is claimed.

## FlashVM coordination and remaining work

FLASHVM-0075 independently replays and agrees with0084's narrow published-entry/
cache boundary. It does not activate file CALL/LEA8. Read the object-guard API
proposal without importing it. The accompanying
[owning-source inventory](native64-ownership-inventory.md) identifies C/native
count, marking, root, context and ISA boundaries. It explicitly preserves current
exclusive admission and C-stack/collector retirement behavior; it is not a
partial atomic conversion or a concurrency certification.
FLASHVM0076's native strong-owner scopes/local-shared direction was also read.
The inventory distinguishes native owner records from mapped roots, per-instance
publication from Module/code identity, and explicit cleanup from VM unwinding.

Immutable proof/container/local-kind binding, result-call caught recovery,
general caller graphs and whole-file out-of-line C/RX goldens remain. No APK or
full managed GUI/shell readiness is inferred from this binding fix.

| Target | Available native paths | JIT including C |
|---|---:|---:|
| ARM64 | 166/173 (95.95%) | 173/173 |
| x64 | 166/173 (95.95%) | 173/173 |
| RV64I | 87/173 (50.29%) | 173/173 |
| RV64 + Zmmul | 90/173 (52.02%) | 173/173 |
| RV64 + M | 103/173 (59.54%) | 173/173 |

Counts remain Linux-layout availability, not full-port/operand-form readiness.
The production new-format file gate stays43/173, independently of old opcode
entry availability and the trusted runtime prototypes.
