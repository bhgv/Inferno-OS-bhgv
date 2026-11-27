# INFERNO-0205: ordinary Limbo mount composition

2026-11-16; experiment, base96e7cff5. ARM64 remains first.

The selected `-64l` importing executable now runs ordinary Limbo
`Sys.dial -> Sys.mount -> Sys.open -> Sys.read/Sys.write -> Sys.unmount`.
This uses the actual Inferno scheduler, builtin LOAD/MCALL, generated Sys entries,
kernel mount client and POSIX C libstyx server over TCP. No test-only boot entry
is required for these guest programs. Loading Sys does not itself call init.

## Producer, proof and ownership

The existing private Connection profile selects up to8 methods: dial, mount,
unmount, open, read, write, iounit and werrstr. Mount's exact structural signature
is Int result plus FD-ref/FD-ref/String/Int/String. The generated Sys.m entry
remains authoritative for C identity, hash,104-byte frame and map00e8. Native
arguments occupy64/72/80/88/96; flags are a32-bit Word and spec remains aligned.
No public managed kind, Dis64 section/version/opcode, hash algorithm or Styx
message change is introduced. These structural trees are private admission
witnesses, not a shared managed-ID assignment with FlashVM.

Existing112/144-byte callers keep their bytes. A caller importing open uses a
160-byte frame and one additional FD root at144, with map00df20. Word results
remain at128, disjoint from that pointer and the two durable Word locals. Open
may replace the same FD variable; aliases/multiple independent open results are
not generalized. Existing Connection fields remain independently owned. An open
FD retains its mounted channel after unmount until replacement or retirement.

The reader validates complete bindings, exact maps and argument classes/offsets,
load facts and every call setup before allocating. Branches are only early-return
guards; every continuing path traverses the same ordered call groups. A separate
monotone open-origin witness protects the new FD slot. A separate Connection
origin check prevents an open result from manufacturing dial/Connection facts.
The existing scalar-flow proof still validates Word/array/String initialization.
The compiler now independently proves complete Connection-profile output before
writing it. General module/ADT/import/export serialization is unchanged.

Native construction resolves the actual generated mount entry and compares its
frame/map/function identity. The full-pointer result binder separately permits
mount's104-byte argument frame and open's mapped FD result. It refuses an open
binding to a Word cell or a changed mount frame map before writing the return
address. Sys_mount captures its native string views before releasing the VM,
consistent with the existing namespace wrappers. No GC algorithm changes.

## Evidence

* Nine new compiler cases,18 C/JIT executions with actual GC between quanta,
  beside24 prior Connection executions and240 existing mixed fixed-Sys executions
  per architecture. Mount-only, open/read, failure, Unicode, replacement, nil open
  result, remount, explicit Word flags and independent origin facts are covered.
* 220 new malformed images plus39 prior Connection refusals are rejected before
  any native/heap allocation on x64 and ARM64/QEMU. Wrong offsets/classes/maps,
  private signature fields, result addresses and use-before-definition are
  included. Removing open-origin or Connection-origin checks admits invalid
  images and must produce a semantic admission mismatch, without running them.
* Native guards reject a Word-address binding for open and a changed mount
  callee map in both C/JIT, leaving the return-address slot unchanged. The loader
  rejects wrong generated mount map, size and function before allocation.
* A GC omission skips only the new caller FD root while preserving its descriptor
  and every other mark. The next collection detects sole-owner loss before using
  the freed handle. All9 new constructors are swept through164 native and50 heap
  allocation failures; two live open-result allocation failures close the newly
  obtained descriptor and leave the result nil. Prior cleanup/unwind gates pass.
* Host ASan/UBSan covers the updated owning-runtime/GC/constructor/refusal corpus;
  ARM64/QEMU runs UBSan. The full existing argument, pointer-fact, slice, primitive
  descriptor, tuple and Connection admission omission controls remain exercised.
  The fixture explicitly includes storage/numeric proof dependencies and provides
  a fail-closed stub for the unrelated private helper binder it never executes.
* Complete consoles:28 guest roots per architecture, C/JIT and direct/fragmented
  TCP, covering mount, read, write, FD replacement, missing file, remount and the
  RU/EN tutorial. The proxy independently checks real requests/clunk counts,
  Rread payload and write acknowledgements; server shutdown checks exact native
  handle/client retirement. These emulator runs are regular native builds, not
  whole-VM sanitizer claims. The host fixture is ASan/UBSan with existing lib9
  support, and LeakSanitizer is disabled in this environment.
* 167 prior Sys images equal the preceding compiler byte-for-byte. Nine new
  sources are deterministic, rejected by that compiler and classic Dis-equal.
  Four well-typed unsupported alias/FD/loop sources remain refused. The Command
  producer also passes772 deterministic files,62 older scalar clients,11 helper
  clients,17 importing triples, source refusals and the previous tutorial.
* Prior full consoles pass70 dial TCP connections and8 Connection byte-exchange
  connections per architecture, including C/JIT, failures, replacement and UTF.
  Fixed Sys capture tests pass emu native/WORD32 models under ASan/UBSan. Their
  conversion-phase oracle now reflects held-VM namespace string capture for
  unmount/mount/bind as well as chdir/remove. Original libstyx TCP acceptance still
  passes with the fixture's default arguments.

## Reproduction and remaining work

[English tutorial](dis64-sys-mount.md), [Russian tutorial](dis64-sys-mount.ru.md).
`dis64-sys-fixed-gc.py --limbo ...` includes new owning cases/refusals/omissions;
`--runtime-only` skips additional omission recompilations for focused sanitizer
runs. `dis64-sys-mount-producer.py --limbo ... --baseline ...` checks compatibility.
`dis64-sys-mount.py --limbo ... --root ... --emu ... --server ... [--runner ...]`
runs the complete TCP gate. Build the existing loopback.c fixture with matching
libstyx headers/library; optional arguments choose expected close/client counts
and a synthetic routing username. Default acceptance behavior is unchanged.
`native64-port.py --styx-server ...` enables this live gate; the owning GC gate
already runs through its existing fixed-Sys selection.

The profile remains a bounded, linear executable with early returns, one open
result,32 calls and8 selected methods. Arbitrary FD aliases, multiple independent
FD results, general loops/handlers and general Command/sh/AWM module graphs remain
outside it. Actual16-source AWM preflight still compiles classically and refuses
all four Dis64 trial profiles. Android/Bionic GUI/APK, physical ARM64/RV64,
authenticated cross-VM mount and concurrent cancellation are not established.

FlashVM master955e5a567ca54f4b34adedf806eb2c71441579bc (runtime0.143) adds native
per-connection publication admission/live roots above the shared C library;
its native snapshot remains our28885bf9. Its README explicitly says deferred
Rattach is unimplemented. Our guest client sends ordinary base9P2000, and packet
usernames in the synthetic fixture are not credentials. No grant/VFS adapter,
foreign runtime/scheduler or authority model is imported. The journal reports
this client-side integration; incoming text and the old own prefix stay verbatim.
