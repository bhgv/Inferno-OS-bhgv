# Native64 series0090: transactional Draw descriptor initialization

Production drawmodinit previously assigned each of four global Type pointers
immediately, ignored nil dtype results and ignored builtinmod refusal. A later
allocation failure or ABI registration refusal left partial globals and leaked
descriptors; another initialization could overwrite still-owned descriptors.

The initializer now builds native wrapper maps locally and stages all four
Types. Each dtype failure releases every staged descriptor. Registration failure
preserves its diagnostic across cleanup, with an explicit fallback for empty
diagnostics. Only successful registration publishes the global maps and Types.
No allocation or callback intervenes after registration before publication.
Complete initialization is idempotent; partial state refuses without allocation.
Caller still holds VM initialization exclusion. No Tk/security/crypto changes.

## Actual-source validation

New native64-draw-init fixture uses actual DRef/DFont/DImage/DScreen/DDisplay
layouts, dtype/freetype, nativeptrmap and protected VM error body. Registration
is an injected boundary and GUI destructor identities are stubs: no real Draw
export ABI, graphical object destruction or full GUI startup is claimed.
Independent expected maps check every bit and each real descriptor callback.

Local x64 O3 nonrecovering UBSan passes WORD32 and WORD64. Per model:23 cases
cover all four allocation failures and retry, both registration diagnostics with
and without cleanup diagnostic disturbance and retry,14 partial states and one
complete state. Successful retries check idempotence and exact allocation balance.
Three source mutations (retirement/publication/registration) must reach semantic
status10; compiler/sanitizer/signal/timeout failures never count. After a fatal
predicate, the harness releases only its tracked allocations so intentional
control leaks cannot mask that predicate with LSan. Production cleanup is tested
before this fatal-only harness cleanup; positive cases must balance unaided.
Full Draw consumer syntax passes both host WORD models.

The classic-mk ARM64/x64/RV64 driver adds positive and three control logs.
Focused native ARM64/x64 ASan/UBSan also adds those gates. New target results
are pending publication; source619d58bc from0089 has completed its full matrix
on all three targets and Bionic module/Sys checks. No prior result certifies
this production change.

## FlashVM and remaining boundary

FLASHVM0079 independently replays0089 alias module sets on x64. Read FIR LOCALITY.md
at70f832ac765e5dace6992f4511eb0e684483495e: private homes differ from referent
identities, captured snapshots survive cell rebinding, callbacks/unknown writes/
suspension and CFG boundaries invalidate facts. Agree with conservative proof
direction; no lock elision, publication or heap concurrency is inferred. This is
document review, not replay/import of FIR implementation. Shared Dis64 IDs stay
unchanged.

This fixes owning Draw initialization but does not migrate all Draw exports or
admit AWM. Managed file-call/import ABI closure, immutable result/caller proof
binding and physical Android GUI acceptance remain. Native path counts remain
ARM64/x64 166/173; RV64I/Zmmul/M 87/90/103; native+C173/173. Production file
admission stays43/173, independently of opcode availability.
