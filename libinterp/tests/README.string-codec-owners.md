# Byte/string conversion ownership and packed borrowed inputs

Run from the repository root:

```sh
python3 libinterp/tests/string-codec-owners.py
python3 libinterp/tests/string-codec-owners-jit.py
```

The instruction driver requires Unicorn. The C driver extracts production
CVTAC/CVTCA, constructors, c2string and mem2array, uses real VM declarations and
lib9/rune.c, and reuses the active allocation/ownership predicates from
string-view-native.c. ASan/UBSan and NDEBUG are enabled by default. The only VM
worker runs on a joined pthread so ASan can track its error-frame longjmp stack.
WORD32 changes scalar WORD/UWORD only; all pointers remain native.

CVTCA now counts and encodes borrowed Rune data through aligned local Rune
values. Packed String headers and payloads remain supported. Counting uses the
same encoder as publication, preserving existing replacement of invalid code
points and embedded NUL bytes. The source remains owned until encoding and
allocation finish; no temporary UTF cache or fallible intermediate allocation
is introduced. Existing payload/allocation limits bound the signed byte count.

Both conversions reject misaligned owning result Heap prefixes before reuse,
allocation or destruction, including nil-array input that releases an old
result. CVTCA also validates alignment of an old array's backing owner and
releasable element descriptor before proceeding. Borrowed Array headers and
read-only element descriptors may still be packed: their accessed fields are
copied as bytes. These checks distinguish borrowed input views from the native
objects whose counters/destructors may subsequently be accessed.

AMD64/ARM64 CVTAC and CVTCA fast paths mirror the owning-result guards before
reading result metadata. CVTCA also declines packed result element descriptors
before descriptor reads. Valid ASCII reuse remains native; Rune/UTF conversion,
allocation and generic ownership transitions remain in C through the complete
JIT entries. No new opcode or private ABI is introduced.

Each scalar model passes 55,856 scenarios with ASan/UBSan/NDEBUG:

- 55,296 import/export cases over 16 borrowed owner shifts, ASCII/Rune data,
  lengths 0 through 17, eight result-cell shifts, nil/unique/shared old results
  and deterministic allocation failure. Independent UTF and Rune oracles cover
  NUL, boundaries, surrogates, values above Runemax and full-width invalid keys.
  Packed read-only element descriptors, allocation counts, unchanged source
  images, returned data and balanced references are checked.
- 560 rejected result/backing/descriptor cases. Errors precede armed allocation
  failure, leave caller cells and full result/root/type images unchanged, and
  do not allocate or release an object.

Twelve previous-C-source controls separately fail packed Rune export and five
owning-result families in both WORD models. STRING_SOURCE selects that source;
STRING_VIEW_MODELS selects native/word32; CODEC_OWNER_DIRECTION selects imports
or exports; CODEC_OWNER_TARGET_ROUTE selects an individual rejection family.

Each instruction backend passes 16,384 memory/read/write/image cases over both
WORD widths and every byte shift of borrowed sources, owning results, element
descriptors and result cells. Hooks forbid result-owner or result-descriptor
reads before rejection. Packed borrowed inputs/descriptors continue to succeed.
Six previous-emitter controls independently expose the two owning-result guards
and CVTCA descriptor guard on both architectures. EMITTER_DIR selects previous
headers; STRING_OWNER_ARCHES, STRING_OWNER_OPERATIONS and CODEC_JIT_TYPE_SHIFTS
isolate controls. Published code remains within the existing staging budget.

Adjacent checks include byte-string-native.py, string-bytes-native.py, both
instruction suites, string-bytes-spans.py, string-codecs.py, string-capture-native.py,
complete C/RX dispatch and ARM64 compiler publication. Incoming 73a6b5a is
preserved unchanged; its string-cell-spans-c and utf8-view-spans-c adapters pass
7,260 and 420 cases per WORD model on top of this series.

Coverage remains AMD64/ARM64 173/173 complete JIT+C entries and 166/173 guarded
native opcode paths. These are dispatch/operand-domain counts, not completion
of the native VM ABI. Full compiler/frame/reference-layout migration, VM boot
and physical ARM64 execution remain separate acceptance work.
