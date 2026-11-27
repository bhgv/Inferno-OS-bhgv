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


## Completed source-pinned target evidence

[Address run41584398708](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41584398708)
passes on native ARM64 and x64 at source d69a872137f23d6b16e4a2db3e6a178e83931757.
Read completed job logs and independently downloaded both artifacts: exact ZIP
digests, positive23-case WORD32/64 markers and all three strict controls match.

| Native address artifact | ZIP SHA256 |
|---|---|
| ARM64 13206698917 | ace5dec77a92439724fd4bb16bf2434721b737323645f8d57dd969edb36559b4 |
| x64 13207895732 | c23a65ff229b97fa35db8ac1f04733a3703bc040ac03863bbc899b71a548559f |

[Android run41584542703](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41584542703)
passes at launcher6a584c0aa2ef537336d590a2fceb6dfdfd351016 with that exact core.
It compiles full Draw with NDK r27c and executes the new actual-body descriptor
fixture/controls on Bionic/QEMU, followed by all binding/Loader/module-set/Sys
and whole-process C/JIT checks, exact packaged images and real worker retirement.
This remains scalar FB startup; registration/destructor boundaries in the new
fixture remain injected as documented above.

Downloaded and verified [APK pair/evidence](https://github.com/bhgv/Inferno-OS_Android/actions/runs/41584542703/artifacts/13207762265).
ZIP SHA25691dfa7c295607c7d1ba8975faff986c67aa01560c73b418707c972d51f0e5bad.
Both APKs are4617038 bytes, no DEX, ELF64/AArch64 with16KiB LOAD alignment and
new series0090 package identities. The bundled scalar image remains
9138ae33fb041adb8b0b689ad1e5c1420fdb5d86a33f0c4dad6c6e96b05c18e0.

| Mode | APK SHA256 |
|---|---|
| C | 2c01ccf7c3e816dd38edadd9466b24ed68053185e0931154a51a0e8bd23794c2 |
| JIT | 267ad95c6d831454ee32c3bba87cf8dd6177badeb3363c7e5ce3c5b99d7194f6 |

New full Linux classic-mk run41584398790 is still in progress at this report;
ASan/Bionic success does not substitute for its remaining full target results.
Physical phone, AWM and managed file admission remain separate.
