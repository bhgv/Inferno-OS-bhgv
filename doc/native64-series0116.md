# Series0116: display-local mixing, composed Frame64 rollback and wide periodic coordinates

This series fixes native Draw lifetime/arithmetic hazards and tests one complete
Frame64-to-native-to-packet-to-wrapper path. Managed Draw registration and Dis64
execution admission remain closed at their previous boundaries.

## Display-local image mixing and refusal

allocimagemix kept its GREY8 quarter-mask in a process-global static Image pointer.
The mask remained attached to the first Display, leaked its native header, and
could be consulted after that Display closed or while drawing on another Display.
The low-depth texture branch also allocated this unnecessary mask.

Each high-depth mix now owns its own temporary GREY8 mask on the current Display.
The low-depth branch uses that Display's existing opaque image and refuses a
missing/foreign default before allocation. All temporary allocations are cleaned
up on refusal. The successful rectangle and mask value retain the previous
semantics: a one-pixel patch in a replicated2x2 low-depth image, or the original
quarter-mask blend in a1x1 high-depth image. Mask/source retirement leaves only the
returned native result. Per-call high-depth masks add allocation work compared
with the unsafe global cache; no speedup or rendered-pixel benchmark is claimed.

The existing rectangle serializer is exposed only through a private native
_drawimage status helper in draw-packet.h. Public draw/drawop/gendraw/gendrawop
remain void wrappers around the same bytes. allocimagemix can now refuse a failed
packet reservation rather than returning an unfilled result. It also checks
consuming temporary-release status. Local rollback preserves diagnostics and
retires local headers even when transport fails; it is not a remote transaction
or proof that partially accepted commands can be undone.

## Periodic-coordinate arithmetic

drawreplxy previously subtracted Int32 coordinates in int before modulo. Both
max-min and x-min could overflow; a zero period could divide by zero. It now uses
wide subtraction/modulo and narrows only the mathematically bounded result.
For max<=min, it returns min and sets the native diagnostic, avoiding a trap.
Valid intervals retain min-inclusive/max-exclusive semantics across full Int32
endpoints. drawrepl applies that rule independently to both axes. No JIT opcode,
wire layout, public signature or shared feature/ABI ID changes.

## Composed actual-source checks

The attach test closure now includes actual allocimagemix, rectangle serialization,
native retirement, drawreplxy/drawrepl and the actual Display.colormix Frame64
consumer. The bounded allocation/transport adapters retain real allocated Heap/
Type owners; there are no fabricated Heap headers.

New checks perform47 direct native mix calls: alternate two simultaneously live
Displays/depths, refuse every temporary allocation, refuse one draw reservation,
refuse flush, reject missing/foreign defaults, then close one Display and mix on
the other. Observers decode each real packet while source/mask owners must still
be alive and belong to its Display. Temporary native/managed ledgers restore.

Ten composed Frame64 cases pass through the real consumer, native factory,
packet, managed Image wrapper and rollback. They include packed output, result
aliasing the display argument, three native-allocation refusals and actual managed
wrapper allocation failure. Displaced result ownership is observed separately.
This is direct bounded Frame64 C execution, not a guest managed CALL or GC proof.

An independent Python unbounded-integer oracle checks1006 interval/coordinate
triples through both actual coordinate functions, including full-range subtraction,
negative wrapping and invalid/zero intervals. Local O3 nonrecovering UBSan with
platform aliases passes WORD32 and WORD32-file:38,579 predicates/model, previous18
attach plus10 composed colormix cases, all Type/managed/native ledgers restored.
All23 strict omission controls pass, including mask/source retirement, packet
refusal, low-depth geometry, coordinate behavior and wrapper rollback omissions.
Signals/compiler/sanitizer failures/timeouts never count as semantic detection.

The separate existing primitive suite also passes3,000 independent native/Frame64
cases and2,209,253 predicates/model, plus all seven controls. This verifies the
serializer's packets and existing fifteen consumers after the status refactor.
The complete actual libdraw archive rebuilds via classic mk. Focused native ARM64/
x64 ASan+UBSan and full target gates consume the enlarged closure; exact-source
hosted acceptance is pending at publication. CI budgets increase for the expanded
checks without dropping any control: attach30min, existing owners35min, full90min.
The prior full x64 job took almost57min before this expansion; a60min budget left
little room for the added compilation cohort. Runtime performance is not inferred.

No renderer, real device or collector executes in these bounded native adapters.
Fixture teardown explicitly breaks its default-image cycles. Bounded Draw consumer
count remains84/89; managed registration stays closed. No Android/AWM activation,
new APK or whole managed-module/root-lifetime acceptance is claimed.

## Prior target receipts and FlashVM coordination

Prior d20d8bf source passes native ASan/UBSan ARM64 job126271658358 and x64
job126271658247 in41737877414: actual logs read,28,865 predicates/model and all16
previous controls. Independent-binding job126271658343 also passes with actual
log read:25 identities,23 structural passes/two malformed outer, four controls.
These accept the previous name/ID boundary source, not this new mixing code.

Older f3b1f5f full41731794855 is now complete/pass on all three targets:
ARM64 126255980452, x64 126255981129 and RV64/QEMU126255980843. Actual logs were
read, including classic mk/build/boot/lease, prior attach gate and128 complete-
emulator live-local processes/two wrong-answer controls. This is exact previous-
source target evidence, separate from current focused/native acceptance.

Read FLASHVM-0117 at830fadc747d4951376a21cc86eaa640639ddc044. Its eighteen
native integer branches/guarded target-read work and measured costs stay peer-local;
WORD64/NativeWorld/TSan results do not establish our Int32/Ref64 Frame64 contract.
Independent typed-role/initialization/result/LEA validation on the same immutable
corpus remains agreed/pending before owning execution. No private21 semantic or
execution authority follows from the existing25/23/2 structural checkpoint.
No shared wire/feature/ABI ID, default43/173, ordinary WORD32 LEA or immutable named
-X dis64-calls -d launch/cache agreement changes; no new format decision is needed.

Linux ARM64 console build/execution already has target evidence. Full managed
imports, roots/collector/resource integration and Android/AWM startup/graphics
remain open. Native fast counts stay ARM64/x64 166/173(95.95%), RV64M103/173
(59.54%); all173 dispatch entries have C support. These are not whole-port readiness
percentages, and no unsupported APK completion date is assigned.
