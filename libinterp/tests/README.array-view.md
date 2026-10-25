# Unique array slices on AMD64 and ARM64

SLICEA narrows an existing view or pointer-free root in place when its heap
reference count is exactly one. The view already owns its element Type and backing root, so changing
its length/data pointer requires no allocation, owner increments, decrements or
GC barrier. This works for scalar, mapped and zero-width elements. The original
root still owns every backing element, including elements outside the narrowed
view; they are released only when the root itself is destroyed.

The C interpreter validates ordinary slice bounds and arrayspan geometry before
checking source/root lifetime and positive Type references. A unique view or pointer-free root
then updates len/data and returns. Mapped roots and shared owners keep the
existing allocation/retain/publication path. Unique views can operate with
maximum positive Type/root counts because no extra owner is acquired. A pending
allocator failure cannot affect this allocation-free path.

AMD64 and ARM64 add guarded native SLICEA for that same unique-owner case.
They capture full signed WORD start/end and native pointers before any stores,
check len/type/root/ref metadata, and preserve REG and all bytes outside the
view len/data fields.
For positive element widths, end must be nonzero and end*size must fit INT_MAX;
native address sums must not wrap. Zero-width elements preserve their data
pointer, including nil/H, and retain full WORD-sized counts. The valid positive
width [0:0] case falls back to C. Every decline is write-free. Successful native
execution writes only the WORD len field and native data pointer, in that order.
No element bytes, root identity, descriptor counter or collector state change.

The private layout appends arrayroot and typeref, both populated from real
offsetof declarations. The ctypes layout is extended simultaneously and checked
against independent C sizeof/offsetof probes. Header fields must have distinct
spans, and descriptor size/ref fields must be distinct aligned C ints. Both
emitters keep the existing private 64-unit staging bound. RV64 stays on C.

Evidence:

- array-view-native.py: 3552 cases per WORD model (7104 total), actual C and RX
  AMD64, unaligned operand slots, operand/header aliases, scalar/mapped/zero-width
  backing arrays, unique/shared/root owners, saturated/dead counters, allocation
  failure and exact REG/header/slot images. Mapped elements outside the current
  view remain owned until root destruction; cleanup restores all live/Type counts.
- array-view-jit.py: 76800 independent image/write/geometry/alias/FP cases per
  backend (153600 total), pointers above 4 GiB, eight header byte offsets, both
  WORD models, INT_MAX span boundaries, native address wrap and write-free declines.
- array-view-layouts.py: 18720 layout/capacity probes per backend (37440 total),
  staging and output canaries under ASan/UBSan.
- Native view and self-load matrices also pass ASan/UBSan. Existing empty-view
  tests now require header reuse after the first root-to-view slice. Shared/root
  allocation failure and Type-retain checks retain their separate coverage.
- The core suite has 94 host drivers. The ctypes guard selection passes 1171
  instruction/ABI checks across AMD64, ARM64 and RV64. The publication fixture
  now includes SLICEA in its 79-instruction module, checks target availability,
  and resets its unsupported module using the actual array length. RX permission,
  allocation/protection failure, retry and unmap checks run on all three backends.

There are 139 guarded native commands / 34 remaining on AMD64 and ARM64, out of
173 active Dis commands. RV64 M, Zmmul and I remain 103/70, 90/83 and 87/86.
The root extension broadens SLICEA without adding an opcode ID. Native support remains partial;
WORD32 uses native 64-bit pointers and legacy WORD64 is also checked. ARM64
execution is emulated, with physical target execution still outstanding.

## Pointer-free roots and copying

Root reuse additionally requires Type.np == 0: discarded elements must not own
references. Array.root stays H. Array.data may advance, while destruction still
frees the original Heap allocation. No Type, Tarray or root retain is needed.
Mapped roots remain on C allocation paths; mapped views remain eligible because
their backing root continues to own every element. Both emitters validate the
existing typenp offset without changing the private layout ABI or 64-unit bound.

Ten extra C/RX scenarios per WORD model force allocator failure during two
successive root shrinks, then copy the narrowed root, share it, create a view,
copy that view and release every owner. Widths 0, 1, 3, 8 and 17 check payload
geometry and original-allocation destruction. These 20 cases pass ASan/UBSan.
[Array-copy source validation](README.array-copy-source.md) covers the related
source-lifetime and byte-span checks.
