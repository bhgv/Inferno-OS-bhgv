# INFERNO-0164 — FlashVM: Sys dial through the Inferno Dis64 loader

2026-11-13. Follow-up to FLASHVM-0172 / INFERNO-0163. Source base:
Inferno experiment `f06042d9d0926b34e71e347c4a99159999428067`.
Also read the newer FLASHVM-0174–0176 notes. This work follows the user's
priority: port the existing Inferno to 64 bits with JIT, first a working Android
client. It imports neither DisVM nor its proxy/GC/concurrency implementation.

## What is connected now

Limbo `-64l` can emit a complete bounded `Sys->dial` caller. The existing file
loader admits its entire call sequence before allocation, resolves the actual
generated Sys entry/hash and uses ordinary LOAD/linkmod/MFRAME/MCALL. Sys startup
selects 23 fixed exports, now including dial. Loading is still separate from init.
The original 14-schema String/Word/FD/byte-array file group is unchanged; dial has
its own explicit tuple proof and result binder.

Caller frame: 112 bytes, map `00 dc`. Strings: FP64/72. Result: status Word FP80,
unmapped padding FP84..87, FD/FD/String roots FP88/96/104. Native dial frame:
80 bytes, map `00 c0`, full borrowed result address at 32. New
`native64diallea8step` accepts only this caller/result/callee geometry. It rejects
a mutated return address targeting FP88 before writing the borrowed result slot.
The tuple tree uses private witness 0x77, not a new public kind or shared ABI.

The source proof currently accepts at most 32 complete calls, literal/nil String
arguments, the same local result tuple and Sys reloads between calls. Subsequent
Limbo field/status expressions, other imports and branches are still refused.
Five valid ordinary-Limbo sources with extra effects are checked to be refused
by this producer, including a trailing LOAD and a second independent tuple.
This is an owning call/result step, not general ADT or network-library admission.

## C ownership bug found and fixed

Forced real GC during cleanup exposed a gap in existing `makeconnection`:
a first adopted FD lived only on the C stack while a later allocation failure
released the VM during cleanup. It could be collected before rollback destroyed
it. The old standalone FD fixture did not run that collector path.

A private `SysConnectionStage` Type (size sizeof Connection, three native roots,
map e0) now owns the partial FD/FD/String. The stage is initialized empty and
registered with existing Inferno ptradd before acquiring children. Each child is
published into the stage before another fallible step. Rollback closes raw FDs
while the stage remains rooted; root-registration failure retires the still-empty
stage before any releasing cleanup. Successful construction transfers all three
owners to the mapped caller tuple, empties/unroots/retires the stage, then releases
old owners. Heap/Type/frame public layouts and wire records have not changed.

The common C helper also serves announce/listen, but their Dis64 file admission
was not enabled. Sys pipe retains its existing construction path. Native failure
clears the previous Connection; VM construction failure preserves its managed
payload while status may already be -1. Do not treat the 32-byte result as one
atomic transaction.

This is an Inferno exclusion/ptradd fix. Please review its external-root and
publication operations against your native host's own concurrent policy before
refreshing the corresponding adapter. It is not new TSan/concurrent-GC evidence
for FlashVM, and no global exclusive/reference fallback is proposed.

## Validation performed here

- AMD64 ASan/UBSan and ARM64 UBSan under QEMU: 16 dial C/JIT/GC cases each,
  including Unicode/nil arguments, success/failure and owner replacement.
- 16 stage/first-FD/second-FD/String allocation failures with and without a previous
  result; 2 runtime mapped-result binder refusals; 45 malformed full-image
  refusals before allocation; partial caller-construction rollback.
- Both active omissions (tuple roots and tuple return address) are detected.
  The existing 240 fixed Sys C/JIT/GC cases and their 8 omission controls pass.
- Full console on AMD64 and ARM64/QEMU: 70 real TCP connections each, C and JIT
  requests, repeated tuple replacement and EOF after replacement/retirement.
  Malformed dial images are refused before scheduling. ARM64 classic-mk build
  and the full paired-console gate pass, including this new loopback gate.
- Updated C-wrapper owner matrix under ASan/UBSan: emu WORD32 and WORD64 each
  4294 cases; unchanged os port WORD32 and WORD64 each 4102 cases.
- Selected Sys registration: 23 entries, 51 allocation refusals plus existing
  metadata/link/rollback controls. The coherent generated interface still comes
  from module/sys.m and limbo -H.

C/JIT here describes C execution and requested JIT lifecycle on the same VM;
it does not claim every instruction was machine code. ARM64 QEMU evidence is not
Android/Bionic or device GUI acceptance. Network callbacks/GC and field/result
checks in the owning fixture have controlled OS boundaries; the separate console
loopback uses real sockets. No authenticated proxy protocol, distributed GC,
remote function/result capability or NC64/DIS64 interoperability is claimed.

Reproduction and bounded Limbo example:
[English](dis64-sys-dial.md) / [Русский](dis64-sys-dial.ru.md).
