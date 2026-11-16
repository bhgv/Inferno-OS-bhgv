# Inferno / DisVM Styx interoperability

2026-11-16. FlashVM master19d8c84b8db3e35741de44263abb4bf7b7c62949
imports native lib9/libstyx from Inferno experiment4029a622 via
`disVM/native/UPSTREAM.json`. All23 selected libstyx files, four conv codec
sources and public Styx/fcall headers match our current source bytes. The
library is shared C code, not a second protocol implementation to translate.

The network protocol remains **base 9P2000**, independent of the host ABI:

| Field | Wire representation |
| --- | --- |
| Packet size | uint32 little endian, including the header |
| Type / tag | uint8 / uint16 little endian |
| fid / newfid / afid | uint32 little endian, even on LP64 |
| Qid | type:uint8, version:uint32, path:uint64 |
| File offset / length | 64-bit little endian |
| String | uint16 byte length followed by UTF-8 bytes |
| Directory data | complete counted stat records inside Rread |

Do not transmit `sizeof(Fcall)`, pointers, native `ulong`, frame/ADT layouts or
Dis64 records as a network message. Those native structures are not a wire ABI.
Limbo channels are also not a substitute network serialization format.

## Repeatable cross-feed

```
python tools/libstyx/tests/wire-interop.py \
  --peer /path/to/flashVM \
  --lib9 /path/to/native/amd64/lib9.a
```

The runner compiles **each repository's own four original C codec files** and
headers, using the selected lib9 archive only for remaining native support.
Independent Python vectors cover all27 legal base message types100..127 except
illegal Terror106; extra Tversion/Rversion variants bring the total to29.
For each packet, every truncated prefix must be refused. Each codec independently
re-encodes to the original bytes, and both cross-feed orders must preserve the
complete corpus. Vectors include high-bit32-bit fids, high-bit64-bit Qid paths,
64-bit offsets/lengths, binary write data, UTF-8, Rstat/Twstat and directory Rread.
Negotiation variants test codec representation, not server policy.

Use `CC`, `CFLAGS`, `--arch arm64` and `--runner 'qemu-aarch64 ...'` for ARM64.
The support archive must match the target architecture. Host ASan/UBSan and
ARM64/QEMU UBSan pass; support lib9 archives were existing native builds, so
this is not a whole-library sanitizer claim.

## Server/session evidence and useful reuse

The existing native64, stream, session, directories and lifecycle C tests were
also compiled from the DisVM snapshot against our native support archive and
passed with host ASan/UBSan. The Inferno independent Python TCP test drove the
DisVM-source C POSIX server; the DisVM test drove the Inferno-source server.
Both passed actual two-connection live directory/file/stat/write operations,
independent msize/views and orderly close/free. The split-point unit test, not
TCP send boundaries, proves deterministic fragmentation handling. Leak checking
was disabled because this execution environment cannot run LeakSanitizer.

DisVM's native package is useful as a standalone build/test adapter for the same
C libraries. Its explicit adapters supply native thread-local errors, formatting
and decimal synchronization. Our experiment already includes the upstream fixes
it uses: full-width fids/Qids, safe callback counts on LP64, fragmented frames,
negotiation/session authority, failed-client refusal, walk errors, live directory
records and transport lifetime. No VM/runtime migration is needed for these.

Base negotiation chooses9P2000, including a request for a dot-suffixed variant;
it does not implement9P2000.u-specific extra fields. Tauth is explicitly
unsupported; packet uname/aname are not authenticated credentials. A connection
must negotiate before attach; negotiated msize bounds complete packets and
reversion closes existing fids. Rebuild native consumers with the same public
Client/header layout. Backend callbacks execute on their service thread; do not
copy a foreign scheduler or invoke guest code from arbitrary transport threads.

These results establish the covered library wire/session compatibility. They do
not establish a complete DisVM VFS-to-Inferno mount, authenticated remote service,
Android/Bionic transport or GUI/APK startup. A real mount adapter must preserve
attach authority, tags/flush, per-fid lifetime and error/close behavior. Refer to
DisVM's STYX-NATIVE/STYX-SERVING docs for that separate integration frontier.
