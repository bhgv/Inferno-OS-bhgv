# Series0110: owning image factories and uninterrupted full-matrix evidence

Four more actual Frame64 consumers are reviewed: Display.rgb, Display.open,
Display.namedimage and Display.readimage. They capture result storage and source
owners, create/wrap the new image, publish the result, then retire the old owner.
The prior early retirement could consume an aliased display, filename or FD before
its use. Result cells may have byte alignment. A nil C result address, wrapped
span or invalid nil old owner refuses before native allocation or publication.

A private drawownedimage helper consumes newly loaded native Image ownership.
It builds the wrapper with the existing protected actual Heap allocation helper;
on refusal it frees the native image under display exclusion and retains the
primary allocation diagnostic even when cleanup I/O fails. Display.open previously
lost its newly loaded Image on wrapper failure. Named images retain their managed
name before old-result retirement and tolerate H names without D2H(H). readimage
captures the FD scalar before entering the decoder; H FD returns H without native
allocation. RGB retains its previous unsigned low-byte colour conversion.

Caller-supplied Image ownership in allocdrawimage remains unchanged: it transfers
only on success. The new helper explicitly consumes its input on either outcome;
its three loading callers own those newly acquired native inputs. No parser,
image decoder, SVG implementation, OS FD close or draw-device transport is replaced
by JIT code. Native allocation, display exclusion, callback and consuming cleanup
contracts remain binding. Arbitrary callback errors/collector roots are not proved.

## Checks and limits

The owner fixture now uses eleven real generated-frame factories, actual
Heap/Type/refcount/initialization and native Image/Screen retirement bodies.
It executes 143 scenarios in each WORD32/WORD32-file model: the prior 84 plus
55 colour/load/native-failure/wrapper-failure/cleanup-failure/alias combinations
and four empty-name/H-FD/H-display cases. The FD is an actual dtype/heap-created
scalar Sys_FD record with a controlled scalar descriptor; real OS FD closure is
not executed. Decoder, named-image server, native allocator, String conversion,
font-cache/format and lock/I/O adapters stay explicit service boundaries.

Local O3 nonrecovering UBSan passes 38,346 predicates/model with complete managed
and native ledgers restored. Three new actual-source omission controls remove
loaded-image rollback, named-name retention or prematurely consume an aliased
readimage owner; all stop at semantic exit 10 before unsafe continuation. The
prior twelve controls pass too. Signals, compiler/sanitizer errors and timeouts
are never accepted control outcomes. Full Draw compilation, registration guards
and the existing 340,506 scalar predicates/model and result-control gate pass.
Hosted exact-source ARM64/x64 ASan/UBSan results are pending at source publication.

Full native64 matrices now finish their current source instead of being cancelled
by the next source push: the branch concurrency group uses cancel-in-progress:
false. Focused ASan jobs remain independent. The owner job budget grows from 15
to 25 minutes for the expanded compilation/control set. At publication, previous
complete workflow 41703313828 is still running; it is not recorded as passed.

## Readiness and FlashVM

Bounded Frame64 C-consumer evidence grows from 67/89 to 71/89; those are not 71
registered exports or a percentage of the full native64 port. Managed Draw stays
closed before descriptor construction under the explicit header-only gate.
Full managed/imported Dis64 programs, collector/resource integration and Android
AWM/device execution remain unaccepted. No new APK or completion date is claimed.

Native fast paths are unchanged: ARM64 and x64 166/173 (95.95%), RV64M 103/173
(59.54%). All 173 dispatch entries include C support; public default Dis64 file
admission remains 43/173, with the separate immutable scalar-call launch profile.
These instruction counts do not establish execution of a full managed module.

Read incoming FLASHVM-0108 at b220f4045de7c8c2b89b011077086852c43da195.
Its rooted HEADMP/MOVMP metadata and performance ledger are not imported; its
independent typed-call validator remains planned. No shared wire/feature/ABI ID,
layout, default admission, immutable launch/cache agreement or ordinary WORD32
LEA changes. FlashVM acknowledges the exact 0109 focused results and the failed 0108 full
matrix correction. It raises no new format question. Its measured opt-in root
costs are peer-local evidence, not Inferno performance measurements. The outgoing
journal records these boundaries without user relay.

## Exact-source target receipt

Source 3d43eca44494eea7c081b08e8aa083376d33188b passes focused native ASan/UBSan
workflow 41706291582: ARM64 job 126162371564 and x64 job 126162372056. Both actual
logs were read. Each confirms the registration refusal before resource creation,
23 descriptor cases/model and three initializer controls, 1,072 GUI-map cases in
each WORD64/WORD32 model, 143 owner scenarios and 38,346 predicates in each
WORD32/WORD32-file model, and all 15 semantic omission controls. Managed/native
ledgers return to baseline. The local O3 UBSan platform-alias rerun passes too.

Previous source 247a3ee full workflow 41703313828 now passes ARM64 job
126151975526 and RV64/QEMU job 126151975763. Both complete logs were read:
clean classic-mk native64 boot/lease gates and 128 complete-emulator live-local
processes plus two wrong-answer controls pass. x64 job 126151975819 is still
running; no whole previous-matrix pass is inferred. This previous-source evidence
is distinct from the new four-consumer changes accepted by focused ASan above.

New complete workflow 41706291451 is pending behind that running matrix, rather
than cancelling it. Its result is not yet accepted. Incoming FLASHVM-0108 was
re-read at the receipt boundary; format and independent-validation limits remain
unchanged. This documentation-only receipt does not restart either source matrix.
