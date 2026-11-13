# INFERNO-0165 — usable Connection fields and byte I/O

2026-11-13. Continuation of INFERNO-0164, code base 8568f34b. The newer
FLASHVM-0177–0179 journal entries were read and preserved while advancing the
local documentation base to 297c480f. Their hosted proxy/result protocols and
concurrent runtime are not imported here. This remains the existing Inferno
Word32/ref64 port, with Android ARM64 the first application target.

## Completed source-to-runtime path

The checked `-64l` producer now composes dial, read, write, iounit and werrstr
in one Sys import group. A Limbo program can inspect dial status, pass c.dfd or
c.cfd, use c.dir as a String, allocate byte buffers, read/write, slice to the
returned count and convert between bytes and String. It can keep a byte buffer
across another dial and then write through the replacement data FD.

A new 144-byte caller has exact map `00 df 00`: String arguments 64/72, dial
status 80, Connection roots 88/96/104, array/String roots 112/120, scalar result
128 and saved Word locals 132/136. Tuple and scalar result addresses and pending
callee geometry are independently checked. The third wire descriptor is the
checked byte witness; existing canonical Tbyte adoption precedes JIT compilation.
No new public kind, opcode or wire field is introduced. Previous two-Type,
112-byte dial-only images and the original 14-schema Sys file profile remain.

The loader replays definite initialization separately for Connection/status,
array/String and Word values. A scalar result cannot invent a Connection; FD
roots cannot become Word counts; pointer argument stores define no Word local.
Calls are complete, guards can only return early, and no branch can enter a
partially prepared call. Source fields are inline Connection value offsets,
not a dereferenced ref Connection and not a foreign network capability.

The new producer still has explicit bounds: a current Connection/array/String,
two saved Words, literal/nil String arguments and linear calls with conditional
returns. General loops/branches, copied Connection ADTs and field mutation remain
refused. Array temporary-slice aliases retain the prior bounded identity rule.
This is now a data-exchanging client, but not general Limbo/ADT admission.

## Evidence collected in this series

- AMD64 ASan/UBSan and ARM64 UBSan/QEMU each pass 24 new C/JIT/GC executions:
  status guards, data/control FD selection, directory, empty/nil buffers, UTF-8,
  short/error reads, byte/String round trip, stable saved counts and replacement.
- Six allocation errors with a live Connection cover array replacement, read-text
  conversion and conversion back to bytes. Previous tuple fields/owners survive;
  eventual retirement balances both FDs and all Type/heap owners.
- 39 malformed-image refusals precede allocation. All 18 native and five managed
  allocation points of a mixed caller constructor are refused with full rollback.
- Three new deliberate omissions (Connection initialization, FD-as-Word and caller
  roots) fail the required semantic predicate. All ten existing Sys/dial controls
  still pass. The 240 fixed Sys and 16 previous dial C/JIT cases remain covered.
- Full consoles on AMD64 and ARM64/QEMU each complete eight real TCP connections:
  request/reply-prefix echo, UTF-8 request bytes, buffer reuse after reconnect and
  two failed-dial guard runs, with C and JIT requested. A single read is not claimed
  to be complete protocol framing; controlled C cases check exact short counts.
- Full paired-console suites pass on both architectures. Sys registration still
  verifies 23 entries and 51 allocation refusals. 209 previous image cases are
  byte-identical against the prior compiler. The RU/EN tutorial source compiles.

C/JIT denotes the existing C and requested-JIT lifecycle, not proof that every
instruction ran as native code. QEMU is not Android/Bionic/device acceptance,
physical memory ordering or performance evidence. Our GC/ownership proofs use
Inferno's existing VM exclusion; they do not certify a concurrent FlashVM adapter.
The stage-root and guarded-publication distinction in FLASHVM-0177 is retained.
No DVP version, proxy lease, remote result image or peer frame layout is adopted.

## Android startup frontier

The AWM preflight now compiles all sixteen startup sources ordinarily and tests
both `-64` and `-64l`. All sixteen ordinary builds succeed; both Dis64 routes still
refuse this broader closure. Scalar mode reports managed LOAD; the private managed
route reports general provider/caller shapes and the shell module/exception table.
This is compiler evidence only, not AWM execution. The remaining critical path is
broader managed entry/parameter frames, library bodies/import closure, exceptions
and required variadic calls. A linked GUI archive or this working console client
must not be described as a runnable AWM APK.

[English tutorial](dis64-sys-connection.md) · [Русский](dis64-sys-connection.ru.md)
· [Android build status](android-arm64-gui-build.md).
