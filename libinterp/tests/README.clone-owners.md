# Mapped clone owners and wider record copies

`newmp` validates descriptor identities, signed size/map counts, mapped spans,
and nonempty source/destination identities before copying or initializing data.
The same map checks run before array allocation and before a module instance
allocates its MP. Rejection therefore cannot leave uninitialized mapped data
for error cleanup. Mapped fields use `memmove`, so native pointer fields need
not be aligned.
A nil mapped owner, dead heap reference or absent heap type raises `exHeap`.
Non-array owners reserve a reference before the mark barrier and publication;
a saturated counter raises `exHeap`. Arrays retain their recursive deep-copy
semantics without acquiring a reference to the source array. Partial output
contains only owners already acquired, so existing protected callers can destroy
it safely after a later field fails. Source and destination remain separate
clone buffers; this does not add an in-place clone contract.

`Sys.tokenize` rejects negative capacities, minimum signed lengths and absolute
lengths exceeding capacity in either the source or delimiter. Validation occurs
before either payload is read, words are allocated or the result tuple changes.

AMD64 and ARM64 `MOVMP`/`HEADMP` now copy pointer-free records through 256 bytes.
The overlap-aware byte loop, private 64-unit staging bound and fallback contract
are unchanged. Records of 257 bytes or containing mapped pointers use C. This
extends two existing opcode paths; it adds no opcode IDs. Current coverage is
139 native guarded paths / 34 remaining on each backend, out of 173 active Dis
commands. RV64 M, Zmmul and I remain 103/70, 90/83 and 87/86. Both WORD models use
native 64-bit pointers. Guarded support is not complete VM/native ABI support.

Evidence:

- `clone-owners.py`: 1280 cases per WORD model (2560 total), including zero,
  maximum and last-legal heap counts, repeated aliases that exhaust capacity
  after one retain, malformed maps/types/buffer identities, all eight field
  alignments and exact source/output images. Cleanup restores each acquired
  reference. The fixture rejects the parent `newmp` body.
- `module-data-retains.py`: 17 cases per WORD model, including negative,
  oversized and partial-slot maps; invalid MP descriptors allocate nothing.
- `token-capacity.py`: 128 cases per WORD model per emu/os error runtime
  (512 total), checking both representations, both inputs and eight result
  alignments. Invalid descriptors cause no allocations or tuple stores. The
  fixture rejects the parent tokenization body.
- `mapped-copy-native.py`: 226576 cases per WORD model (453152 total), comparing
  real AMD64 RX entries and production C handlers against full byte images.
  Expanded sizes include 129, 255, 256 and 257 bytes, overlaps and operand aliases.
- `mapped-copy-jit.py`: production AMD64/ARM64 bytes run with Unicorn/Capstone,
  above 4 GiB: 59620 cases per backend (119240 total), with exact images,
  write-free declines and preserved FP state.
- `mapped-copy-layouts.py`: 106080 layout/capacity probes per backend.
- Clone-owner, token-capacity and native copy matrices pass ASan+UBSan; the
  sandbox emits its known nonlocal-jump sanitizer warning. These matrices do
  not reproduce the separate module-instantiation rethrow redzone failure
  described in README.cross-call-retains.md.

ARM64 execution is emulated; physical ARM64 hardware has not been tested.
The full core suite and runtime publication fixture are run alongside these
focused matrices. Tk, security, crypto and MIPS/SPARC/PowerPC emitters are outside
this series.
