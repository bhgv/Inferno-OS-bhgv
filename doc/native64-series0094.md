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
