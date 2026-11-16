# INFERNO-0206: create/remove and namespace calls in the guest mount profile

2026-11-16. Base874bdfd4, integrated experiment16c387c1 without conflicts.

Ordinary Limbo `-64l` programs can now compose `Sys.create`, `Sys.remove`,
`Sys.chdir` and `Sys.bind` with dial/mount/open/read/write/unmount. The existing
native Inferno handlers, generated Sys exports, scheduler and kernel namespace
remain authoritative. No replacement VM or FlashVM ownership model is imported.
The private profile admits12 selected methods and32 calls; all12 imports occur
in one tested executable. Create uses the same mapped FD result at144 as open;
its generated80-byte callee packs mode/permission at72/76. Bind uses88 bytes/map
00c0; remove/chdir use72/map0080. Complete signatures, frame maps, argument/result
addresses and load/origin facts are checked before construction. No public
Dis64 kind, section, hash algorithm, opcode or base9P2000 wire change.

Evidence:176 previous Sys images (including9 mount sources) remain byte equal.
Six new sources compile deterministically, fail on the preceding compiler and
produce identical classic Dis. Four well-typed unsupported sources still refuse.
Host ASan/UBSan and ARM64/QEMU UBSan pass54 Connection C/JIT/GC executions,
including12-import composition and15 mount constructor cases:282 native and93
heap rollback points.343 malformed Connection images refuse before allocation,
including84 new create/remove/chdir/bind argument/result offset mutations.
Open/create each have live allocation failure and result-address guards;
wrong mount callee maps refuse before writing the return slot. Existing240
fixed-Sys executions, dial cases and active GC/flow omission controls pass.

Each full emulator runs44 TCP roots (C/JIT x direct/fragmented) across11 modes.
New coverage creates a synthetic file, writes12 bytes, replaces the create FD
with an open FD, removes its name and reads the retained channel. Relative open
after chdir and read through bind use the real kernel namespace. Tests inspect
request/clunk counts, Rread bytes, Rwrite count and server close/client balance.
MCREATE and server root write permission are required. The create path's initial
walk receives one expected nonexistent-name Rerror; successful Tcreate follows.
No host files are used by the synthetic backend. Emulators themselves are regular
native builds; the host server is ASan/UBSan with LSan disabled, not a whole-VM
sanitizer/leak claim.

RU/EN runnable tutorials: dis64-sys-files.md/.ru.md. Identical code blocks are
compiled and executed as the eleventh TCP mode on both architectures. The older
mount tutorial remains byte-equal as source code and also executes. Original
libstyx native/session/stream/fid-context/directories/lifecycle/deferred-attach
regressions and default TCP loopback pass under host ASan/UBSan.

The integrated peer commit16c387c1 adds native deferred attach/flush cancellation
and a POSIX readiness descriptor. Existing synchronous attach continues to work;
no managed authority/debug adapter was imported. Incoming FlashVM journal and
existing own journal prefix remain verbatim. Our compatibility is base9P2000
messages through the shared C library, not serialized native pointers or grants.

Limits remain explicit: one independent open/create FD root with replacement;
FD aliases/two independent results are still refused. General loops, sh/AWM
module graphs, Android/Bionic GUI/APK execution and physical ARM64/RV64 acceptance
are not established by this batch. AWM classic sources still compile while the
four experimental profiles refuse; the Android client is not yet ready.
