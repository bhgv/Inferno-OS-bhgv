# Series0112: protocol-width channels, full subfont paths and executable peer-schema checks

Chans.text and Chans.mk now use their reviewed generated Frame64 declarations
without assuming result alignment. text captures the descriptor and old result,
finishes actual String allocation, then publishes before old-owner retirement.
Allocation refusal preserves the caller's old String. mk finishes String conversion
and native parsing before byte-copying the complete result. Native64 managed Draw
registration remains closed before descriptor construction.

## Native 32-bit channel protocol on 64-bit C hosts

The draw protocol stores at most four byte-sized channel components. A 64-bit
native ulong previously admitted eight valid components to chantodepth/chantostr;
formatting could then write sixteen characters plus a terminator into a short
caller buffer. strtochan could also accumulate extra components and lose them when
assigned to a 32-bit VM result. Native width is now explicitly bounded by the
32-bit protocol before formatting. Parsing rejects more than four components,
zero/overwide component widths and invalid total pixel depths. Valid channel
strings retain their previous values and whitespace/token handling. Invalid strings
still have depth zero, as documented in man/2/draw-display. This strengthens native
API validation; it allocates no new Dis64 ID or representation.

## Owned complete subfont paths

subfontname previously copied the font path into a fixed 256-byte stack buffer
and truncated later joins/probes into fixed buffers. It now computes bounded full
lengths before addition/allocation, captures the complete base path in one owned
allocation and probes the complete .3/.2/.1/.0 candidates in existing depth order.
The successful candidate or unsuffixed fallback transfers that allocation to the
caller. Relative paths without directories, root directories, UTF-8 bytes, absolute
names and *default* preserve their intended behavior. Native probing/FD-close
services remain consuming nonthrowing callbacks; no real device/font rendering or
collector/root lifetime proof is inferred from the bounded test adapters.

## Actual C and Frame64 verification

A new fixture uses production Heap allocation/destruction, String constructors,
UTF conversion, retstr and both channel consumers, plus actual libdraw channel and
subfont-path code. Allocator capacities, error text and file probing are controlled
services; actual Heap/Type headers are allocated rather than fabricated.
Local optimized nonrecovering UBSan with platform aliases passes 4,107 cases and
234,873 predicates for each WORD32/WORD32-file model. It covers arbitrary native
widths before formatting, canonical/malformed tokens, ASCII/Rune source ownership,
packed and overlapping result cells, UTF-cache and returned-String allocation
refusal, complete long paths, depth order and allocation rollback. All tracked
allocations retire and String Type references return to baseline.

Four strict semantic omission controls allow wide descriptors, bypass canonical
parsing, retire an old String before publication or alter the probe suffix. The
wide-descriptor control stops at the depth oracle BEFORE unsafe formatting; signals,
compiler/sanitizer failures and timeouts never count as control success.
Complete Draw compilation and the existing 340,506 scalar predicates/model pass.
The actual libdraw archive rebuilds through classic mk. New native ARM64/x64
ASan/UBSan jobs and the full port pipeline include this fixture; exact-source
hosted results remain pending at source publication. Bounded Draw C-consumer
coverage grows 80/89 to 82/89, not a module-registration or whole-port percentage.

## Independent Dis64 schema evidence made repeatable

Read FLASHVM-0112 and its separate schema commit
92f2dafdb21e7554ea91b3de679f56505762bf5b. Its independent Python reader has been
reviewed and replayed locally on the existing 25 immutable Inferno call images.
All byte identities and the exact published partial report match: 23 canonical
outer directories, two malformed outer containers and 23 metadata/types/Rune32
schemas. Code/exports, cross-section bindings, typed roles and execution remain
unvalidated by that reader. Its published actual C-inspector refusal report is
peer-reported evidence, not a C-inspector execution performed by this new Python gate.

utils/tests/dis64-peer-schema.py checks the exact peer source/report hashes and
our generator identity before importing the external reader. It runs the reader's
four legal/seven malformed section checks, then independently mutates corpus byte
identity, validly rehashed malformed metadata and manifest locals/admission labels.
The last mutation must not become schema/type/execution authority. Three controlled
reader omissions prove the gate detects failures at the intended semantic stage.
Both the normal gate and all three controls pass locally. CI checks out the exact
reviewed peer revision separately; no peer implementation is vendored into the VM.
The new quick schema job saves its limited scope and unvalidated fields in JSON.

FLASHVM-0112's new native integer writer guards and measured costs are not imported.
Native WORD64 guard behavior remains distinct from Inferno Int32/Ref64 Frame64 and
the shared wire contract. Default 43/173 admission, ordinary WORD32 LEA and immutable
named dis64-calls launch/cache discrimination remain unchanged. FlashVM typed-call
execution is still planned; partial schema acceptance does not enable it.

## Prior complete targets and remaining work

Prior exact source 559b08ca full workflow 41715744069 now passes all three targets:
ARM64 126199241481, x64 126199241968 and RV64/QEMU 126199242051. All actual logs
were read, including classic-mk libdraw/build/boot/lease gates and 128 complete-
emulator live-local processes/two wrong-answer controls. This is previous-source
acceptance, distinct from the new channel/path code and its pending target gates.
Native fast counts remain ARM64/x64 166/173 and RV64M 103/173, with C support for
all 173 dispatch entries. Full managed Dis64/imports, collector/resource integration
and Android/AWM execution remain open; no new APK or completion date is claimed.

Read concurrent FLASHVM-0113 at 1a8867cd before publication. Its compact native
range metadata and measured mitigation remain peer-local; the independent reader
checkpoint intentionally stays pinned to 92f2daf. No wire/ID/admission change or
unmeasured Inferno speedup is inferred from those native metadata changes.
