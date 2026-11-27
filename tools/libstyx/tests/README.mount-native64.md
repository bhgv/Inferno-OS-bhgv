# Real native64 Inferno mount client acceptance

2026-11-16, INFERNO-0204. This advances the codec-only compatibility gate to an
actual Inferno kernel mount against the original POSIX C libstyx server.

## Production fixes

* `mntversion` formerly freed its reply buffer and then used `f.version`, which
  borrowed bytes inside that buffer. The live x64 probe failed the version return
  check with the old code. It now uses the retained `m->version` copy.
* Version negotiation previously treated one transport read as a complete reply.
  `mntversionread` now reads the four-byte length and exactly the remaining body,
  preserving the next packet. It bounds the declared size, rejects EOF/oversized
  transport counts and advances the channel offset by actual bytes received.
* Tag bitmap storage and temporaries are explicitly uint32, with unsigned shifts.
  `ulong` is64-bit on LP64; `1<<31` was undefined signed arithmetic even on32-bit
  targets. Tags0 and65535 remain reserved; no wire format changes.

## Acceptance

`mnt-tags.py` extracts the actual bitmap declaration and allocator/free bodies.
It allocates all65534 usable tags in order, checks exhaustion, frees/reuses every
other tag, releases all tags and repeats. UBSan passes on x64 and ARM64/QEMU;
the signed-shift omission is detected on both.

`mnt-version-read.py` compiles the actual production reader with deterministic
transport adapters. It covers31 fragment sizes, every incomplete packet prefix,
short/oversized/high-bit declarations, insufficient capacity, oversized adapter
counts, exact offset accounting and leaving the next message unread. Host
ASan/UBSan and ARM64/QEMU UBSan pass. An early-return omission must fail an active
semantic check on both targets.

`mount-native64.c` runs only in a separate acceptance build, before guest
scheduling. It uses real `kdial`, `kfversion`, `kmount`, `kopen`, `kread`, `kwrite`,
`kseek`, `kstat`, `kclose`, `kunmount`, namespace channels and the production mount
RPC/multiplexer. It does not replace codecs or the kernel/transport functions.
The probe checks:

* negotiated base9P2000 and mount after explicit version negotiation;
* closing the original connection fd while derived mounted channels retain it;
* an open directory, data file and control file;
* initial data and EOF at a64-bit offset above32 bits;
* a Qid path with its high bit set through real Tstat;
* a control write changing backend state, followed by live data on the same fid;
* complete directory stat data, changed name/mode and directory EOF;
* closing all handles, unmount and exactly-once server connection retirement.

`mount-native64.py` temporarily includes that probe in the **prepared build's**
boot source, builds and copies an isolated probe binary, and restores/rebuilds
the ordinary emu in finally. It runs direct TCP and a proxy that sends the first
real version reply one byte at a time. The deterministic unit establishes exact
fragmentation coverage; TCP send boundaries alone do not. A second fixture
control connection stops the original server and checks four closes/two clients.

Both direct and fragmented runs pass on Linux x64 and ARM64/QEMU. The x64 client
uses our current server; ARM64 uses the peer-source server from FlashVM
master3b67db2021bcf579dc174abea7f92c4c93da9cd5. Both share Inferno source28885bf9.
All28 selected current libstyx/codec/header/test files match that peer snapshot.
The new per-fid context test also passes host ASan/UBSan. Existing Sys.dial
full-console tests pass70 real TCP connections per target after ordinary emu is
restored, including C/JIT and managed tuple refusal/retirement checks.

The kernel/emulator probe builds are regular native builds, **not** whole-VM
sanitizer runs. The synthetic native server is ASan/UBSan, with existing native
lib9 support; LeakSanitizer is disabled in this environment. Fixture usernames
select synthetic views, not credentials. No host user files are served.

## Reproduction

Prepare a disposable current native64 emu root with `/mnt`, and build the
existing `loopback.c` server with its matching libstyx headers/library. Set PATH
and other build variables for that root, then run:

```sh
python emu/port/tests/mnt-tags.py
python emu/port/tests/mnt-tags.py --signed-shift-control
python emu/port/tests/mnt-version-read.py
python emu/port/tests/mnt-version-read.py --early-return-control
python tools/libstyx/tests/mount-native64.py \
  --root /path/to/prepared/root \
  --emu /path/to/prepared/root/Linux/amd64/bin/emu-g \
  --server /path/to/native/loopback-server
```

The mount runner defaults to `mk CONF=emu-g install` in root/emu/Linux; override
`--build-command` if needed. `--runner` selects QEMU for the emulator; the server
runs on the host. Unit tests use CC/CFLAGS/NATIVE64_RUNNER where applicable.
Use a native support lib9 archive of the correct architecture when building.
The four focused unit/omission gates are also included in native64-port.py.

## Remaining integration

This proves real native64 **kernel-client** mount interoperability, beyond
packet roundtrips. It does not yet enable ordinary Dis64 Limbo `Sys.mount`: the
selected generated Sys profile/serializer must admit its descriptor/string/flag
signature before guest programs can use this path. Flush/cancel concurrency,
authenticated remote grants, Android/Bionic/device/GUI/APK and physical ARM64
acceptance remain separate. No foreign VM/runtime/scheduler is imported.

Peer runtime0.141 now adds file-grant-to-fid adaptation with independent native
handles. We retained its upstream28885bf9 C API extension unchanged: per-fid
context ownership changes native embedding layout, not9P messages. Rebuild
matching Client/Styxops consumers. The prepared-grant VFS adapter belongs to
DisVM; no runtime/authority model was copied into Inferno.
