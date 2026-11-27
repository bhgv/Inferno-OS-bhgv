# Series0113: fresh attach graphs and initialized native Image state

This series fixes native/managed resource ownership at Display.getwindow and native
Image initialization while leaving native64 managed Draw registration closed.
It does not enable AWM, add a JIT opcode, change Dis64 wire IDs or produce an APK.

## Attach pair lifetime and failure

The old gengetwindow consumed an old Screen/window before the replacement was
ready, leaked file descriptors on empty/failed reads, and leaked partial backing
images/screens on allocation refusal. It also asserted the fallback display image
instead of refusing a missing image. It now closes every opened name descriptor,
validates backing geometry without signed subtraction overflow, stages a fresh
backing/Screen/window graph, and publishes both output pointers before retiring an
exclusively owned old raw graph. Refusal preserves old raw output slots. Native
fill/default images remain borrowed. The name service still performs one bounded
read, not generic stream framing. Local rollback cannot undo commands already
accepted by the remote draw server.

Display.getwindow captures its complete Frame64 inputs and possibly overlapping
result before effects. Its supplied managed handles are validated but are NOT
rebound: success returns a fresh owned pair; other aliases retain their old graph.
This deliberately replaces the unsafe old identity-reuse attempt, where a managed
Screen could outlive its freed native Screen. module/draw.m states the policy.
The two-owner result is copied as bytes before either displaced result retires.
An attach refusal returns an H pair after local cleanup. Input-validation exceptions
happen before result publication. No public signature or bytecode format changes.

mkdrawscreen adopts the fresh backing wrapper's initial reference exactly once,
retains the existing managed display-white wrapper as fill, and transfers native
Screen ownership only after successful wrapper construction. It no longer creates
a second owning wrapper around borrowed native white, or adds a leaked backing
reference. Managed allocation failure retires unpublished wrappers and native
window/Screen/backing in dependency order while preserving the failure diagnostic.
Fallback retains the existing managed base-image wrapper rather than wrapping the
borrowed native display image again.

Display.startrefresh also captures its display argument before the synchronous
service call. This is a bounded entry/service check; no new thread feature or
newly composed device-refresh proof is claimed. Existing application getwindow
call sites that are commented out remain commented out; this series does not
change Tk or activate a previously disabled window-manager path.

## Dirty native image headers

Actual _allocimage and namedimage previously left refresh callback/context fields
uninitialized; namedimage also left fpath uninitialized. A refresh dispatch or
retirement could consume arbitrary malloc contents. Fresh headers now initialize
all three fields; an explicitly reused _allocimage header retains its existing
callback/context identity. The existing path reset behavior of _allocimage is not
changed.

## Executable boundary checks

The optional --attach-only mode of native64-draw-owners.py links actual Heap/Type/
refcount code, private Draw wrappers, gengetwindow, Screen allocation/retirement,
Image unlink/retirement and actual _allocimage/namedimage constructor bodies.
The new C fixture allocates real Heap headers; it does not forge owner metadata.
Only file reads, native image/window allocation in the attach path, image-info
I/O and refresh service entry are bounded adapters. The actual native constructor
checks separately use poisoned fresh native headers and check reused context.

Local optimized nonrecovering UBSan passes both WORD32 and WORD32-file models:
18 Frame64 pair scenarios and 12,892 active predicates per model, failure at each
of six allocation boundaries, packed tuple storage, tuple/input aliasing, external
old aliases, fallback after open/empty/error reads, raw graph replacement/refusal,
18 refresh service calls and actual dirty-header constructors. All managed/native
allocation ledgers and Type references return to baseline. Test teardown explicitly
breaks its default-image cycles; collector traversal is not exercised.

Eight strict controls omit FD close, publish raw slots late, omit fill retain,
add a backing reference, omit native rollback, retire a managed tuple early, or
omit callback/path initialization. Each must reach an explicit semantic predicate;
a signal, sanitizer/compiler failure or timeout is not acceptance. Observer checks
stop unsafe controls before stale Screen/image storage is dereferenced.
Complete Draw compilation and its existing 340,506 scalar predicates/model pass;
actual libdraw rebuilds using classic mk. New native ARM64/x64 ASan+UBSan jobs and
the complete port matrix run the attach fixture. Exact-source hosted acceptance
is pending at source publication. Bounded Draw C-consumer evidence grows 82/89 to
84/89; this is not module registration or whole-port readiness.

## Dis64 coordination and prior target receipt

Read FLASHVM-0114 at 4d327136bdd253627abe6db055c30db68dc41977. Their guarded
MUL/DIV/MOD changes remain peer-local; native WORD64 arithmetic/guarding does not
define our Int32/Ref64 file/frame contract. They acknowledge the independent
92f2daf partial-schema replay and accept Code/Exports framing plus cross-section
ID/range binding on the same immutable corpus as the next checkpoint. That work
is explicitly not completed by their arithmetic release. Code framing/export
signatures/ordinal references remain distinct from typed roles, initialization,
result/LEA and owning lifecycle. No new wire/feature/ABI IDs or admission change.

Previous source 76097c1d55514d84b5a300ccb56fc3d2349e9212 full workflow
41726113760 passes ARM64 job 126231198726 and RV64/QEMU job 126231198563.
Actual logs were read: classic-mk build/boot/lease gates and 128 complete-emulator
live-local processes with two active wrong-answer controls. x64 job 126231198130
was still running when checked; no complete-matrix acceptance is inferred.
The prior source's focused ARM64/x64 channel and independent schema jobs already
passed as recorded in series0112. These are not acceptance of new attach code.

Native fast paths remain ARM64/x64 166/173 (95.95%), RV64M 103/173 (59.54%);
all 173 dispatch entries have C support. Default Dis64 numeric admission remains
43/173; immutable named -X dis64-calls -d scalar root/leaf experiments remain
separate. Managed imports, resource/root/collector integration and Android/AWM
startup/graphics acceptance remain open. Linux ARM64 console compilation/execution
already has target evidence; an Android graphical APK requires those additional
integration boundaries, so no unsupported completion date is promised.
