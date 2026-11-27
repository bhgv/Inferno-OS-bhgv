# Native64 series 0094: selected interfaces and real Freetype registration

The real Sys/Draw/Freetype input previously made -H emit all builtin interfaces,
including a Draw_Rect_size metadata/function collision irrelevant to the font
consumer. New optional -H -M Freetype selects the owning interface and retains
its actual referenced Draw_Point type. It does not change default -H bytes or
pretend the complete Draw interface is fixed. Missing/duplicate/empty selections
and unsupported selected dependency shapes refuse before output.

The complete production Freetype consumer now supports an explicit generated
int32/ref64 header and checked six-export builtinmod64 registration. Qualified
method/frame names agree with actual compiler output. All four descriptor owners
and private maps stay staged through constructor failures. Header-only mode,
non-WORD32 or invalid registration options cannot fall back to legacy tables.
newmemface remains explicitly unimplemented.

Optional Linux64 --native64-freetype-boot builds the fresh selected header/font
archive through classic mk and initializes the coherent module during real
startup. Console devices remain unchanged; only the headless font archive is
added in dependency order. Configuration clears optional closure/model residue
on deselection and preserves existing outputs on invalid requests. Android's
scalar startup stays builtin-free; its separate target harness tests the module.

Local O3 UBSan with synthetic port aliases:754 predicates per WORD32/WORD32-file,
21 real constructor/allocation refusals each, all six published exports invoked
with fresh frames and real Vera/Noto/native-handle work. Three strict semantic
map/frame/retirement controls, header-only/packed refusal, five compile guards
and seven compiler selection/dependency cases pass. Actual constructors, maps
and destructors run; bounded heap/ASCII/error/exclusion transports do not certify
production moving GC or scheduler transitions.

A fresh classic-mk x64 emulator links the real font archive and boots JIT0/1 with
Freetype registration,38 corpus processes/1096 checks and four import/image
refusals. Compiler regression passes four native tools/72 Dis-AOT source cases;
full native64 declaration/constructor/call suite passes. Default Math/Sys -H
headers match previous compiler bytes. Existing legacy graphical initializer
syntax/control gates remain passing. Fresh native/Bionic target results follow.

Prior0093 full classic-mk41602133654 now passes native ARM64/x64 and RV64/QEMU;
its sanitizer/APK results remain separate. This series does not change native
fast counts (ARM64/x64 166/173, RV64I/Zmmul/M87/90/103), complete JIT+C173/173
or scalar file admission43/173. No Tk/security/crypto or excluded JIT backend
implementation is changed. No shared Dis64 IDs or native pointers on wire.
See dis64-native64-freetype-register.md for exact reproduction and boundaries.


## Peer ownership review

Received FLASHVM0087/0088/0089 through baf4d3d5 and read the immutable call,
service and JIT count contracts. Owning x64/ARM64 pointer-cons counter sequences
remain plain; RV64 has no corresponding fast path. Preferred shared direction
is a checked C ownership/builder contract with explicit widths, source lifetime,
roots, marking, refusal and rollback, before proven per-target CAS lowering.
No atomic helper/fallback was imported. Owning MCALL's cached count sequence
needs an independent future call-gate regression; the hosted gate insertion
and new Freetype export fixture do not certify that boundary. Full response
is in the outgoing journal. Shared format and serialized admission stay.


## Source-specific native and Android evidence

Production2d92a79e658faa9315a2b98e53d81e483d7f2169 passes native ARM64/x64
ASan/UBSan41611046071. Both source-specific artifact ZIP digests and registration
logs were checked independently: six published exports, all21 allocation
refusals/two real fonts per WORD32/WORD32-file, three semantic controls,
header-only/packed and compiler/configuration refusals. The native logs report
758 predicates per model; local754/peer748/Bionic756 reflect ASCII path lengths,
not inconsistent acceptance or failures. No LSan opt-out was added.
ARM64 artifact13219635797 SHA256:
0afe2d3dc95396008b96c45dca078fbba4ba52eb1fbbf4d447e00f27277724c4.
x64 artifact13218549867 SHA256:
11b5c73ed7c929235e756936f0fd37d5ccd0f5252af1d872269848fd9f4aeb78.

Android41611575016 succeeds at launcherbbe8202da0572dac7d7bd55887c818ad540bb9a8,
pinning2d92a79e. Independent Bionic log verification confirms actual registration/
calls/fonts, both frame models, all21 allocation refusals and three controls.
Artifact13218929385 ZIP SHA256:
2713c6617e4ab308219aa14506beb0c7cd9b5cbde7e2e3f8eb8050645544f791.
Both4621134-byte APKs were checked for exact source/package identities, sidecar
hashes, ELF64/AArch64,16KiB LOAD,noDEX and unchanged bundled scalar bytes.
JIT0 APK SHA256:
69a52be848d9672ba74ff863c5fc105d26da5a4b7734e9fe49a3d7430b6f105a.
JIT1 APK SHA256:
220d9e3ba2c0fe3091ad7bffbedda1968958a482d64e492e3da9578fbc6682e3.
Download: https://github.com/bhgv/Inferno-OS_Android/actions/runs/41611575016/artifacts/13218929385
APK execution remains scalar FB/builtin-free; full module/font execution is a
separate Bionic executable, not AWM or phone scheduler/namespace acceptance.

Full corrected classic-mk41611046060 is still running on ARM64/x64/RV64. Local
x64 real-profile startup is already verified as above; source-specific native
address/Bionic success is not a substitute for the remaining full matrix.
Readiness173-row WORD32/64/capacity check passes with unchanged opcode counts.

## FLASHVM0090/0091 receipt

Received incoming through b2a9f35b. Read OWNER-SLOTS runtime0.74 replacement and
MANAGED-OWNERS runtime0.75 direct-capture contracts. Stable publication storage,
tracked displaced owner, preflight-not-reservation, no callback/longjmp gap,
source/root separation and queued Fast retirement remain explicit obligations.
This is documentary review, not source import or TSan/runtime replay.

0091 agrees with the checked C transaction/builder-first direction and the
separate owning MCALL regression requirement. Its independent fresh-Limbo x64
replay passes all0094 registration matrices/controls with748 path-dependent
predicates. No wire/API IDs or parallel managed mode are approved by that review.

The final evidence commit restores the pre-existing executable bit of
native64-scalar-boot.py; its bytes remain those tested at2d92a79e. Only docs/mode
change, so repeated source tests are unnecessary.


### Full ARM64 and RV64 result

At final review,41611046060 completes successfully on native Linux ARM64 and
RV64/QEMU, including actual classic-mk font startup and the complete port gates.
The x64 full-matrix job is still running; its local full startup and native
ASan/UBSan results above remain independent passed evidence.
