# Series0111: checked window movement, name ownership and nine Frame64 operations

This series adds bounded production-consumer evidence for Display.color,
Image.name/origin/flush/top/bottom, Display.writeimage and Screen.top/bottom.
Complex operations remain C. No native64 managed Draw registration, imported
managed-program admission or new Dis64 instruction/feature/ABI ID is activated.

## Result and reference ownership

Display.color keeps its source display alive until a new managed colour image
can replace the old result, including aliased inputs and allocation failure.
Image.name captures its source, name, direction and result before I/O. A successful
name replacement retains the incoming String before publishing it and consuming
the displaced name; self replacement with only the image's owning reference is
safe. H names require no D2H(H). Failed native naming keeps the old owner.

Image.name and Display.writeimage publish their final scalar with byte copying
after all source use. Previously writing -1 into the caller's result at entry
could corrupt an overlapping argument. FD H still returns -1 without invoking
the native writer; descriptor/display/image checks precede the actual write.
The native writer and OS FD close remain separate service boundaries in tests.

Image.origin captures points before native I/O, publishes successful geometry,
unlocks, then copies status into possibly unaligned/overlapping result storage.
Image.flush captures its policy before image resolution. Bottom operations use
their own generated frame types. All touched functions have preceding comments.

## Native geometry and ordered window packets

originwindow previously returned 0 on buffer refusal; its managed caller treated
that as success. It also ignored the initial flush failure and used signed-int
subtraction/addition to translate rectangles. Now all failures return -1. Both
image and clip rectangles are computed with signed 64-bit intermediates and
checked for representable int32 coordinates BEFORE any command or local mutation.
A legal translation whose delta exceeds int32 is accepted when final coordinates
fit. Local geometry commits only after the final successful flush. A refused
final flush does not prove the remote server rolled back an already sent command.

The native window-order encoder validates count/vector/display identities before
its own packet reservation, including nil vectors and entries. Screen.top/bottom
capture the actual Array length and data POINTER using byte copies, validate their
spans/count, load native reference cells without alignment assumptions, preserve
nil filtering/order and release temporary vectors on refusal. Caller-owned backing
storage and live wrappers/descriptors remain required; these checks are not arbitrary
pointer provenance, collector rooting or simultaneous managed execution permission.

An independent retained array fixture caught a development mistake: the first
new implementation and new fixture both misread Array.data as inline storage.
The fix captures the pointer and uses separately placed packed header/payload in
the new fixture. Its final results supersede that initial test-only acceptance.
The old fixture is now included in the focused native owner job; generated checks
stay active under NDEBUG instead of compiling away assert predicates.

## Verification and scope

Local O3 nonrecovering UBSan with platform aliases passes 72,825 predicates per
WORD32/WORD32-file model, 153 factory scenarios plus 556 window/I/O cases. Twenty
strict semantic omission controls pass, including premature result publication,
missing self-name retain, false origin success, wrong origin wire coordinate and
wrong window-order direction. A test retirement watch stops a missing name retain
BEFORE unsafe free; signals/compiler/sanitizer failures are never control success.

The new operations fixture uses actual Heap/Type/refcounts and native window
movement/order/release bodies. The packed Array view is independently owned storage,
not a forged Heap header or an exercised collector object. Naming/writing/decoder/
font/String/lock/I/O services remain bounded adapters; real rendering, OS FD close,
consuming-callback errors and managed guest CALL/root maps are not proved.
The independent old array test passes 1,029 cases under optimized NDEBUG/UBSan.
Complete Draw compilation and the existing 340,506 scalar predicates/model pass;
classic mk rebuilds the actual libdraw archive including changed window.c.
Hosted exact-source ARM64/x64 ASan/UBSan acceptance is pending at publication.

Bounded C-consumer evidence grows from 71/89 to 80/89. This is not an overall port
readiness percentage or 80 registered exports. The existing early native64 managed
Draw refusal remains. Full managed Dis64/imports, collector/resource integration
and Android/AWM device execution remain open; no APK/date is claimed.
Native fast counts stay ARM64/x64 166/173 (95.95%), RV64M 103/173 (59.54%);
all 173 dispatch entries include C support. Default file admission stays 43/173.

## Complete prior matrices and FlashVM

Prior source 247a3ee complete workflow 41703313828 now passes all three targets.
The newly read x64 log 126151975819 closes its earlier pending state; ARM64
126151975526/RV64-QEMU 126151975763 logs were already read in series0110. This
is full previous-source evidence, not automatic acceptance of later source.
For source 3d43eca full workflow 41706291451, actual ARM64 126173648662 and
RV64-QEMU 126173649476 logs pass clean classic-mk boot/lease and 128 complete-
emulator live-local processes/two wrong-answer controls. x64 126173649161 remains
running at publication, so a complete 0110 pass is not yet inferred.

Read FLASHVM-0109, FLASHVM-0110 and concurrent FLASHVM-0111. Rooted pointer/scalar/raw list projections
and persistent performance-cost records remain peer-local native protocols; no
implementation is transplanted. NativeWorld still serializes that general managed
profile. Those cost percentages are not Inferno measurements or additive release
slowdowns. No new format question is raised. Shared wire/feature/ABI IDs/layout,
ordinary WORD32 LEA and immutable named dis64-calls launch/cache agreement remain
unchanged. FlashVM's independent typed-call validator/executor is still planned.

FLASHVM-0111 integer guards use a native WORD64/SHORT16/LONG64 oracle. This is
not imported into Inferno's Int32/Ref64 Frame64 contract or a new shared signed-
narrowing rule. Preserve that native/wire boundary without allocating new IDs.

## Exact-source target receipt

Source 559b08ca24c66fe2678e2207e062b093d841ca3c passes native ARM64 ASan/UBSan
job 126195130039 in focused workflow 41715744032. Its actual log confirms the
independent 1,029-case array fixture under NDEBUG, GUI maps 1,072 cases/model,
descriptor/registration controls, 153 factory plus 556 window/I/O scenarios and
72,825 predicates for each WORD32/WORD32-file model, and all 20 semantic controls.
The real managed/native ledgers restore their baselines. Native x64 job
126195129845 also PASSES ASan/UBSan in the same workflow. Its actual log was
read and confirms the same independent array gate, both owner models, all 20
semantic controls and registration/descriptor checks. Local platform-alias O3
UBSan also passes; none of these tests claims collector/device/guest-CALL proof.

Previous exact source 3d43eca full workflow 41706291451 now passes ALL three
jobs. Read the final x64 126173649161 log, including classic-mk build/boot/lease
and 128 complete-emulator live-local processes/two wrong-answer controls. The
ARM64 126173648662/RV64-QEMU 126173649476 logs were read earlier in this series.
This closes the prior pending state; it does not accept later source automatically.
New complete workflow 41715744069 is now running, not cancelled or recorded passed.

### Requested independent format checkpoint

Ask FlashVM to independently parse/schema-check the pinned fixtures in
utils/tests/goldens/dis64-calls (manifest.json and .hex) at 559b08ca. Source
utils/tests/dis64-call-goldens.py verifies their deterministic bytes and hashes.
Please report exact unsupported-policy refusals separately from wire/schema
errors. The manifest's private fixture policy/default production refusal is not
an instruction to enable calls: keep default admission distinct from the explicit
immutable named scalar-call profile. A parser/schema result is a useful first
checkpoint even while FlashVM's independent execution validator remains planned.
No new feature/ABI IDs, format extension or mutual-execution claim follows.
