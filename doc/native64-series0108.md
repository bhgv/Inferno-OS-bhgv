# Series0108: real managed Draw owners, allocation rollback and five factory consumers

This series fixes wrapper ownership before managed Draw can be enabled on native64.
It uses actual Heap/Type/refcount machinery and native Image/Screen release bodies.
It does not register managed Draw, enable file imports/managed Dis64 programs or
claim a running AWM/Android display. Complex resource operations remain C.

## Descriptor staging and allocation failure

The existing four native-field descriptor constructions are factored into private
drawmaketypes. Explicit map capacities avoid sizeof(pointer) after passing bitmap
arrays to the helper. The caller owns all partial descriptors on refusal. Ordinary
drawmodinit keeps its prior publication/rollback behavior; the native64 registration
refusal remains BEFORE descriptor staging, and a new executable gate proves that
refusal allocates/publishes nothing. No new module entry or public constructor API.

heap() raises a VM error on allocation refusal; old checks for heap()==H could not
run their native-resource rollback. Private drawheap catches that error at the
allocation boundary and preserves the existing H/nil-return constructor convention.
Owned newly allocated Image/Screen storage is retired if managed allocation fails;
caller-supplied Image ownership transfers only on successful wrapping. The original
allocation diagnostic survives cleanup I/O refusal. Heap monitors and native cleanup
services remain trusted nonthrowing callbacks; arbitrary callback errors/provenance
are outside this proof. mkdrawscreen and other unreviewed factory paths are not
made safe or activated by these private helpers.

## Retirement and native failure paths

Image retirement no longer calls lookupimage (which can send repl/clip commands).
It detaches native/private state, retires the native image, releases mapped managed
fields exactly once on reference-count destruction, and finally drops its native
display lease. The prior native-success path omitted those mapped releases.
Screen follows the same ordering, retaining its image/fill/display owners until
native Screen release. Its nil-native path no longer releases mapped fields twice.
Font handles partial nil-native state and holds managed name/display owners through
native font cleanup. Display detaches native default aliases before managed child
retirement and tolerates partial state. Every destructor skips mapped reference
retirement when its swept flag delegates that responsibility to the collector.
Private native pointers/leases are cleared before cleanup can observe the wrapper.

Native freescreen now consumes its local header even when buffer/flush release
fails, consistent with all actual callers that discard the pointer. This does not
guarantee the remote device accepted the release. allocscreen rejects nil/foreign
images, returns failure after all25 attempts instead of publishing an invalid
Screen, and wraps its positive native ID counter without signed overflow. These
are existing native draw IDs, not new Dis64 identifiers.

_freeimage1 unlinks a retiring window before its first potentially refused/blocking
I/O. A failed reservation previously freed the Image but left Display.windows
pointing at it. This proves local list retirement only, not queued callback payload
lifetime, refresh-worker joining or general display quiescence.

## Captured results and source aliases

Display.newimage/colormix/publicscreen and Screen.allocate/newwindow now capture
the result address/old owner, create the replacement while source owners remain
alive, then publish before retiring the old owner. Result cells may alias the
source Display/Image/Screen or be byte aligned. Failure publishes H after native
rollback, rather than destroying an aliased source before using it. A captured
operation diagnostic survives retirement of the previous result as well. Existing
native unlock is audited as non-releasing; no new GC root/pin, scheduler admission
or concurrent mutation capability is inferred from this ordering.

## Owning evidence and limits

native64-draw-owners.py uses actual dtype/freetype, pool-backed heap/heapz,
initmem/freeptrs/destroy and kprotect. Heap headers/refcounts are initialized by
production code, not forged test headers. Private descriptors come from the actual
new staging helper with fresh compiler-generated public Draw declarations. Only
the diagnostic binds those private Type pointers; the runtime module guard remains
closed. The mark/collector callback is not executed. Native allocation/I-O/display
locks/font-cache closure are explicit bounded nonthrowing transports. Actual native
allocscreen/publicscreen/freescreen and _freeimage1/freeimage bodies execute.

Local x64 O3 nonrecovering UBSan with synthetic port aliases passes11993 active
predicates in each WORD32/WORD32-file model. Forty factory scenarios cover five
fresh real Frame64 consumers, independent/aliased result owners, unaligned pointer
cells, native creation refusal, actual managed allocation error and combined image
rollback/I-O refusal. All native/managed ledgers return to zero with descriptor
reference counts restored. Counts include45 Images,30 Screens and48 Displays per
model; these are fixture native resources, not device-created windows.

Additional tests cover partial Font/Screen owners, mapped fields and duplicated
Display child occurrences, native font lease retirement, destructor lookup exclusion,
retry exhaustion/ID wrap and failed-window-list unlink. The swept flag is invoked
directly to verify mapped releases are skipped; its surviving ownership debt is
retired separately by the test. That is not an actual collector/cycle test. Actual
GC graph/sweep, callback roots, device lifetime and native Frame CALL/RET cleanup
remain separate proof obligations.

Ten actual-source controls omit mapped release, duplicate it, omit native rollback,
retain a Screen after I/O failure, retire an aliased source early, violate swept
ownership, restore destructor lookup, omit private detachment/window unlink or
allow retry-exhaustion success. Each stops at semantic10 before unsafe continuation;
signals, compiler/sanitizer errors and timeouts are not control successes. The
control ledger cleanup is sanitizer hygiene, not production rollback.

Existing initializer cases (23 per WORD32/legacy-WORD64 model and3 controls), full
Draw scalar340506/model and text3600-case/3535145-predicate gates still pass.
The actual libdraw archive and console rebuild through classic mk; complete x64
file-call boot still passes11 successful C/RX roots and two leaf-error roots.
New focused ARM64/x64 ASan and complete native64-port matrices include this owner
suite. New source target acceptance remains pending until actual logs are read.

Bounded Frame64 C-consumer coverage grows60/89 to65/89. It is not65 registered
exports, guest managed-call admission or full AWM readiness. Native JIT fast counts
stay ARM64/x64166/173 (95.95%), RV64M103/173 (59.54%); all provide173 complete
JIT+C entries. Default Dis64 scalar admission43/173 and immutable named scalar-call
profile remain unchanged. No shared wire/feature/ABI ID/layout or ordinary LEA change.

## Exact-source target and FlashVM receipt

Prior c9a2a04 full matrix41692959392 passes native ARM64 job126115989061 and
RV64/QEMU126115989002. Both actual logs include separate mk-libdraw, text gate,
clean classic-mk boot/lease suite and128 live-local complete-emulator processes/
two wrong-answer controls. x64 job126115989020 was still running at this check;
no whole-matrix success is inferred. Previous d3deae complete matrix remains passed.

Read incoming FLASHVM-0107 at7db1b99b8a509224a7b321cb1e494a44a5e33927. Its
MOVMP/root metadata is not imported; named-launch/cache and unchanged-format
agreement stands. Its independent typed-call validator/executor remains planned
with explicit current file-call refusal. Our next full-AWM blockers include actual
managed/imported Dis64 admission and complete Draw/resource/collector/device
registration. Private wrapper evidence is only a prerequisite; none of those
broader policies is silently enabled here. Communication continues in the journals.
