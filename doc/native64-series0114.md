# Series0114: bounded device records and independent Dis64 code/binding replay

This series makes native draw initialization independent of native C long width
and integrates the next independently reviewed FlashVM reader. Managed Draw
registration, default Dis64 admission and Android/AWM activation remain unchanged.

## Native fixed-width image information

Both initdisplay and namedimage previously applied atoi to fixed twelve-byte
fields received from devdraw. That accepted partial numbers and made out-of-range
coordinates/client-directory numbers depend on libc and host width. Channel
parsing could also consume adjacent fields because it was given the remainder of
the record rather than an isolated field.

New private drawinfo.h decodes decimal magnitudes with a checked Int32 limit before
multiplication or narrowing. It handles INT_MIN without signed negation overflow,
requires digits and space-only padding inside each twelve-byte field, isolates the
channel token, and validates nonnegative IDs, binary replication and rectangle
ordering. Empty/inverted field shapes and extra channel tokens are refused. Empty
rectangles remain permitted. All checks finish before any Image field or directory
result is published. No public API, draw protocol or bytecode layout is extended.
The header is a classic-mk dependency, not a separate build system.

initdisplay validates the exact 144-byte record before using a directory ID to
open channels. namedimage refuses a short, failed or overlong information reply
and attempts to release the newly acquired server ID; malformed fields use the
same release path. Cleanup preserves the original diagnostic. Remote cleanup is
best effort: a failing transport cannot promise a server-side rollback. _allocimage,
namedimage and attachtowindow now stop before issuing another request when their
initial pending-output flush failed. No new device transaction semantics are claimed.

## Actual-source native checks

The existing attach fixture now compiles the actual shared record parser plus the
actual _allocimage/namedimage constructors. Its independent Python integer/record
oracle covers 514 decimal fields, 400 complete records and five fixed-width channel
cases. It includes Int32 endpoints, overflow, empty/partial/NUL fields, signed
values, bad IDs/replication/rectangle order and dirty output canaries. Refused
records preserve the complete Image and directory output. Native request tests
observe no new packet/ID after a failed preliminary flush and an ID-release packet
following failed/short/overlong/malformed image information.

Local O3 nonrecovering UBSan with platform aliases passes WORD32 and WORD32-file:
15,130 active predicates/model, the prior 18 real Frame64 attach scenarios and all
allocation/Type/native ledgers restored. Twelve strict controls include the previous
eight ownership/initialization omissions plus range-check omission, early parser
publication, ignored preliminary flush failure and missing ID release. Every control
reaches an explicit semantic predicate; signals, compiler/sanitizer failures and
timeouts are not successful controls. The complete libdraw archive rebuilds using
classic mk with the new header dependency. Existing full-pipeline and focused
native ARM64/x64 ASan+UBSan attach jobs consume these checks. Exact-source hosted
results are pending at publication.

Filesystem/native allocation/image-info I/O/refresh services remain bounded test
adapters. This fixture executes the parser, not the complete initdisplay/device
lifecycle. Default-image cycles are explicitly broken by fixture teardown, not by
collector traversal. Bounded Draw consumer count remains 84/89; no managed Draw
registration, CALL admission, device/GC or new APK acceptance follows from it.

## Independent Code/Exports and cross-section bindings

Read FLASHVM-0115 at c481ecb71514d27727198932640aa324dddfe2e1 and reviewed
FlashVM commit 0517104b4192ee0297f58a56a419e6f24e66cde0. Its new independent
peer_call_binding.py/report were fetched and replayed on the unchanged immutable
559b08ca corpus. The three old dependency/report files were verified byte-identical
to their prior reviewed pins at this new commit. No peer runtime code is vendored.

The new integration gate pins all new source/report identities, reuses the old
partial-reader pins and imports only those exact files without adding a checkout
to sys.path. It matches the complete published structural report: 25 byte identities,
23 Code/Exports/Imports/metadata/type/string structural ID/range passes and two
malformed outer containers. It runs the peer's compact boundary/nonminimal operand
checks, sixteen malformed binding cases and unsupported addressing cases. Additional
isolated mutations check byte identity, a rehashed bad export entry, an unsupported
address form and inert manifest policy/locals/admission labels. Four controlled
reader omissions must fail at their own semantic integration stages. All pass
locally. A separate CI job checks out this immutable peer revision and retains its
report/log. The old 92f2daf partial-schema job remains separate and unchanged.

Structural success does not independently confirm the private 21 semantic outcomes.
Opcode/local-slot semantics, function ownership/call roles, exact module-data layout,
entry/export-frame agreement and owning execution remain unvalidated. Unsupported
forms remain distinct from malformed wire. Actual peer C-inspector refusal is
peer-reported evidence, not reproduced by this Python gate. The next requested
independent checkpoint is typed roles/initialization/result/LEA validation before
owning execution. No new wire/feature/ABI ID or unilateral admission is requested.
FlashVM's native guarded-shift implementation is not imported.

## Prior receipts and remaining scope

Source f3b1f5f102cae61b68f69c46a95b886518c6dc95 focused workflow 41731794895 is
complete and successful. Actual new attach-job logs were read: native ARM64
126250762073 and x64 126250761978 each pass 12,892 predicates/model, eighteen
Frame64 scenarios and all eight previous controls under O3 ASan/UBSan. These are
previous-source receipts, not acceptance of this series' new record parser.
Its full workflow 41731794855 remains running at this observation.

Previous source 76097c1d complete workflow 41726113760 now passes all three targets,
including x64 126231198130 (actual log read), ARM64 126231198726 and RV64/QEMU
126231198563. Classic-mk build/boot/lease gates plus 128 complete-emulator live-local
processes and two wrong-answer controls are accepted at that exact source.

Linux ARM64 console compilation/execution already has target evidence. Full managed
imports, roots/collector/resource integration and Android/AWM startup/graphics remain
open. Native fast dispatch counts stay ARM64/x64 166/173 (95.95%), RV64M 103/173
(59.54%), with C support for all 173 dispatch entries. Default Dis64 numeric gate
stays 43/173; named immutable -X dis64-calls -d experiments remain separately
restricted. None of these counts is a whole-port completion percentage or an APK
readiness promise. No unsupported completion date is assigned.
