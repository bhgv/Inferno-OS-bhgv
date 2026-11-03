# Native64 series0097: bounded pixel transfers and line-end packing

Image.readpixels/writepixels previously dereferenced nil byte arrays and passed
unchecked array lengths/backing pointers into native graphics code. They now
validate the owning array header, compatible byte element metadata, signed
length and pointer extent before image lookup can synchronize display state.
They capture a stable byte view, finish the native call/unlock and copy the
complete VM scalar result with memmove, including unaligned/overlapping output.
These checks assume VM-owned object/root lifetime; numeric bounds are not a
proof of arbitrary host-pointer provenance or a moving-GC pin.

loadimage/unloadimage share a private wide pixel-span validator. It checks
rectangle orientation, depth1..32 and capacity, bounds row multiplication by
division and only narrows a result known to fit the supplied int capacity.
Zero-area requests return0 without transport. Nonempty nil storage refuses.
Signed byte offsets use mathematical floor, not overflowing int products or
negative signed shifts. Public bytesperline's truncated result is not used as
an allocation or transfer-size validation oracle.

loadimage now handles a completely clipped rectangle without output, preserves
source stride after clipping and checks output chunk size before transport.
It advances source storage only when another row exists. unloadimage handles
zero stride before division, checks the initial flush, bounds each response to
the requested byte count and completes short reads before advancing rows.
EOF/error no longer silently returns a truncated successful image; native
read failures retain their original diagnostic.
Remote release/acquire remains balanced for ordinary I/O return failures.
This does not add a proof for arbitrary VM longjmp from native I/O/callbacks.

Image.arrow and the native ARROW macro now use unsigned shifts; the VM result
uses a complete byte copy. Negative and large line-end fields no longer invoke
signed-shift UB. The owning scalar fixture executes this eleventh real entry
and independently checks the native macro against the Python bit oracle.

New native64-pixels.py links the actual native load/unload/rectclip sources.
Independent Python unlimited integers supply rectangles, clipped coordinates,
strides, results and source offsets. A bounded transport inspects actual
little-endian packet coordinates/payload and injects short/partial reads, EOF,
I/O/flush/buffer refusals, nil buffers/images/displays, empty/oversized/reversed
rectangles, negative/extreme coordinates and local/remote execution. Exact
owning VM byte-view and two consumer bodies execute with generated Frame64;
image lookup, locks, flush notification and I/O are explicit bounded transports.
Canonical and compatible byte descriptors, ten metadata/refusal shapes,
every scalar output alignment and overlap with rectangle arguments are checked.
No real namespace/device/refresh, image ownership or scheduler is certified.

Local x64 O3 UBSan:96424 predicates per WORD32/WORD32-file plus exact packet
mutation semantic10; synthetic port aliases pass. Complete Draw scalar consumer:
11 real entries,340506 predicates per model, model guards, header-only refusal
and exact result control pass. Native ARM64/x64 ASan and full classic-mk port
matrices include both tests; their new results are separate pending evidence.

The preceding0096 complete classic-mk41619620285 now passes native ARM64,
native x64 and RV64/QEMU, in addition to its native target ASan acceptance.
New scalar/transport frame evidence covers32/89 Draw exports (19 geometry,
11 scalar,2 pixels); remaining57 have compilation evidence only. This count is
not full functional/device coverage or full Draw registration/AWM readiness.
Native JIT coverage and file admission are unchanged; no Dis64 wire IDs/layout,
serialized native pointers or file CALL/LEA8 activation is added.

FLASHVM-0093 was received and preserved. Its pinned POINTER-TAIL.md describes
private hosted SysV x64 JMP into a checked C constructor with the original C
return address, exact TLS REG acceptance, protected decoded operands/roots and
error rollback without decline/retry. Contract review is not an owning source
import/replay. ARM64/RV64 still need their own ABI/root/error lifetime evidence.

## Native target acceptance

Production source: fe5c16e111d631066008dc2a3294ccab3316829f.
Native ARM64 and x64 ASan/UBSan both pass41623878048. Both actual job logs
confirm340506 scalar predicates and96424 pixel predicates per WORD32 and
WORD32-file, plus the exact scalar-result and pixel-packet controls. The new
pixel sources, protocol transport and VM byte-array/frame tests execute on
both native architectures. This does not replace device/AWM or production
scheduler/GC validation. Full classic-mk41623878015 is still running; its
ARM64/x64/RV64 results remain separate pending evidence.
