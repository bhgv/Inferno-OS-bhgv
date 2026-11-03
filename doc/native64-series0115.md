# Series0115: exact name packets, terminal image IDs and safe attach refusal

This series fixes native protocol boundary errors shared by the 32/64-bit Draw
paths. No JIT opcode, shared Dis64 wire ID or managed Draw admission is added.

## Native names and truth values

nameimage previously put strlen(name) into an int, reserved/copy-sent the whole
string, then narrowed its length to the protocol's single byte. A name of 256
bytes therefore advertised zero bytes while leaving the remaining bytes in the
command stream. It now validates an unsigned 64-bit length before int/byte
conversion and rejects names longer than 255 without reserving or sending a
packet. namedimage uses the same width-safe length check before narrowing.
Lengths 0 through 255 retain their existing representation; whether a particular
name is accepted by the real server remains a server decision.

The VM treats any nonzero Image.name flag as true when retaining its managed name.
The native sender previously narrowed that raw int to a byte: for example 256
became zero, so native/server and managed state could disagree about add/remove.
The sender now encodes the existing truth semantics as exactly 0 or 1. Nil/closing
display handles and nil names refuse before dereferencing or sending commands.

## Image counter and attachment

_allocimage and namedimage previously incremented the signed native image counter
without an exhaustion guard. They now reject a negative/exhausted counter before
pending flush or packet reservation. ID2147483647 remains a valid final ID; the
next request refuses without incrementing, wrapping or reusing an old server ID.
The caller still provides display exclusion; these guards are not a concurrency
or general ownership proof.

attachtowindow now has a short explicit native path: validate the handle/display,
stop if preliminary flush fails, reserve/send the five-byte attachment request.
It no longer carries dead commented packet fields/unused geometry or the old
error-goto scaffold. Its public void signature is retained; final transport
refusal remains reported through the existing native diagnostic. It does not
promise remote rollback or change automatic window-manager startup.

## Actual-source evidence

The optional attach gate now links the actual nameimage and attachtowindow bodies
alongside its existing actual constructors, Heap/Type ownership and retirement.
Its bounded transport captures complete legal name packets up to262 bytes. New
checks cover every length0..256 for naming/lookup, a4096-byte refusal, nil handles/
names, seven zero/nonzero truth values, the final legal ID and exhausted/negative
counter refusal, plus failed/successful attachment sends.

Local O3 nonrecovering UBSan with platform aliases passes WORD32 and WORD32-file:
28,865 active predicates per model, the prior18 Frame64 pair scenarios and restored
managed/native/Type ledgers. Sixteen strict omission controls include the prior12
and four new omissions: length validation, ID guard, attach-stop and truth
normalization. The ID guard control is observed at packet reservation BEFORE any
signed increment can overflow; a sanitizer failure is never accepted as its
semantic detection. All controls require an explicit predicate exit, not a signal,
compiler failure or timeout. The actual libdraw archive rebuilds with classic mk.
Existing native ARM64/x64 ASan+UBSan jobs and full target pipelines consume the
extended gate. Exact-source hosted acceptance is pending at publication.

Native allocation, transport/image-info/filesystem/refresh remain bounded adapters.
No real server name acceptance, complete VM managed CALL, collector or Android
execution is inferred. The test explicitly breaks its default-image cycles at
teardown. Bounded Draw C-consumer evidence stays84/89 and managed registration
remains closed. No Tk, crypto/security implementation, Java, or old MIPS/SPARC/PPC
JIT implementation changes are included.

## Prior hosted receipts and FlashVM

Read FLASHVM-0116 at f6a9f832e0d21b5ff4551d3f5e9264e5099a1e30. The peer ran
our exact aef13e7 independent binding gate and all four controls against its pinned
reader/report closure. That is peer-reported reproduction; it does not validate
our private21 semantic outcomes or owning execution. The next independent typed
role/initialization/result/LEA checkpoint is agreed and remains pending. The old
and new immutable reader pins, default43/173, ordinary WORD32 LEA and named
immutable dis64-calls launch/cache discrimination remain unchanged.

FlashVM also isolated a fortified TSan jump/interceptor mismatch on its own host.
Its limited scope and explicit warning against extending that result to ASan or
our whole emulator are preserved. No OS transfer, fortification or sanitizer
configuration change is imported without an Inferno reproduction. This is not a
new concurrency work item in this repository.

Prior aef13e7 source passes native record/attach ASan+UBSan ARM64 job126263529245
and x64 job126263529134 in41735493945: actual logs read,15,130 predicates/model
and all12 previous controls. Independent structural Code/bindings job126263529494
also passes; actual log read,25 identities/23 structural passes/two malformed outer
and four controlled omissions. Other workflow jobs are not inferred from these
focused receipts; x64 owner job126263529337 was still running when checked.

Older f3b1f5f full41731794855 now passes native ARM64 job126255980452 and RV64/QEMU
job126255980843. Actual logs read: classic mk/boot/lease, prior attach gate and128
whole-emulator live-local processes/two wrong-answer controls. Its x64 job126255981129
was still running. These exact prior-source receipts are distinct from new naming
and counter code.

ARM64 console compilation/execution already has real target evidence. Full managed
imports, roots/collector/resource integration and Android/AWM startup/graphics
remain open, so an APK completion date is not supported yet. Fast JIT dispatch
counts stay ARM64/x64 166/173(95.95%), RV64M103/173(59.54%); all173 have C support.
These counts are not whole-port readiness percentages.
