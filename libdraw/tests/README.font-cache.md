# Populated native font-cache regression

Run `python3 libdraw/tests/font-cache.py`. It compiles actual public Draw records,
`loadchar`, `fontresize`, `cachechars`, UTF decoding and Point/Rectangle helpers.
There are24 independent native-table scenarios and979 active predicates per run.
No assertions are used; `-DNDEBUG` leaves checks active.

The fixture allocates exact table capacities with real malloc, poisons new storage
and independently checks every source-copy extent before memmove. It checks
populated growth at1/5/49 entries, eviction at53, allocation-refusal fallback,
retained entries, initialized new cells and balanced source references. It uses
the constructors' real `NFCACHE+NFLOOK` initial table shape, then checks cache-table
replacement/failed replacement, cold batches and warm hash reuse.

Controlled services expose native image initialization and each flush/reservation
refusal. Tests check old geometry/owner conservation, no premature glyph fields,
post-resize glyph refusal, signed bearings and the actual glyph packet's tail.
The existing short Font.width/ushort Cacheinfo.x limits are checked before native
allocation/transport, including a valid representable endpoint.

`--char-model signed|unsigned` changes compiler char signedness. Five controls
(`extent`, `geometry`, `publication`, `flush`, `capacity`) deliberately omit one
production safeguard and must reach a named semantic predicate at exit10. The
source-extent control stops before its deliberately oversized memmove; signals,
compiler/resource/sanitizer errors and timeouts never count as control success.

`NATIVE64_ARCH=amd64|arm64|riscv64` checks actual compiler identity. `CC`,
`NATIVE64_CFLAGS`, `NATIVE64_RUNNER`, `NATIVE64_TMPDIR` select the toolchain and
runner. Local defaults use O3 nonrecovering UBSan. Hosted focused ARM64/x64 jobs
use O3 ASan+UBSan and ordinary leak detection. All modes/controls are included in
the full native64 pipeline.

Native image allocation/retirement, subfont lookup/release, Draw and transport
remain explicit bounded adapters. Image service handles are native records, not
fabricated Heap headers. The ascent-adjustment path is deliberately not exercised.
There is no renderer, server rollback, consuming-callback error, VM collector,
managed guest CALL or root/lifetime proof. These tests do not enable managed Draw
or prove an Android/AWM launch, scale or touch-coordinate fix.
