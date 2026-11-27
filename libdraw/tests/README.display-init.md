# Native display construction rollback

Run `python3 libdraw/tests/display-init.py`. It compiles the actual initdisplay,
closedisplay, refresh discard, fixed-width image-info parser, channel parser and
Rectangle constructor. Native allocation storage is real malloc with poisoned
payload/guard bytes; no VM Heap, root or collector metadata is fabricated.

Current coverage:160 constructor success/refusal scenarios and49,775 active
predicates. The schedule enumerates every normal/fallback allocation/open/read/
channel-conversion refusal. Descriptor0 is valid. Raw descriptors and retained
channel references are independently conserved, rather than treating conversion
as consuming its input. Every partial Image, lock, directory and native header
returns to baseline; native freeimage consumes its header even on cleanup error.

Other checks cover short/empty/extra-length and malformed information records,
optional fstat refusal, local/nonlocal behavior, high-bit64 qid, maximum legal
path with Int32 directory number, oversized names before allocation, independent
device/window directories and two simultaneously live displays. Success returns
with its initial lock held; the fixture releases that lock and runs actual close.
Cleanup deliberately overwrites error text so primary-diagnostic conservation is
checked independently.

`--control memory|channel|image|lock|descriptor|path|diagnostic|complete` omits one
production safeguard. Only exact semantic exit10 plus its expected marker counts.
Signals, compiler/resource/sanitizer errors and timeouts never count. On control
stop the fixture frees only actual tracked allocations; this is exit hygiene,
not production rollback evidence. Checks remain active under NDEBUG.

I/O, FD/channel objects, lock operations, image allocation/retirement, formatting
and debug process identities are explicit native services. Tests do not open an
OS device, start a refresh worker, render pixels or execute a guest managed CALL.
The constructor's actual bookkeeping and close bodies are exercised; arbitrary
throwing callbacks and concurrent users of an unpublished display are outside
the contract. No remote-server transaction/rollback is claimed.

`CC`, `NATIVE64_ARCH=amd64|arm64|riscv64`, `NATIVE64_CFLAGS`, `NATIVE64_RUNNER` and
`NATIVE64_TMPDIR` select the compiler/runner; target identity is checked. Local
default flags use O3 nonrecovering UBSan, focused native ARM64/x64 jobs use O3
ASan+UBSan and ordinary leak detection. The full native64 pipeline runs the
positive fixture and all eight controls. This is not Android/AWM/GC acceptance.
