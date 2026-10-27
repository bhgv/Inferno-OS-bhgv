# String owner preflight in C and the priority JIT backends

Run from the repository root:

```sh
python3 libinterp/tests/string-owner-preflight.py
python3 libinterp/tests/string-owner-jit.py
```

The instruction driver requires Unicorn in the selected Python environment.
The C driver uses ASan/UBSan and NDEBUG by default, real VM declarations and
production helper/opcode bodies. It reuses the bounded allocator, native cache
allocator and active predicates from string-view-native.c. Its VM error frames
run on a joined pthread, keeping ASan stack tracking enabled across longjmp.
WORD32 changes only WORD/UWORD; pointers remain native.

INSC, SLICEC, slicer and addstring now reject packed owning Heap prefixes before
reading typed metadata or changing ownership. splitc checks its native pointer
cell, Heap alignment, live reference, length/capacity and complete source span
before allocating the expanded representation. Invalid ASCII capacity can no
longer bypass checks through expand=1. Failed splits retain the caller's owner,
reference count and bytes. H, empty strings, packed pointer cells and successful
ASCII-to-Rune expansion retain their existing behavior.

INDC borrows its input rather than changing Heap ownership. It reads packed
headers and Rune cells with byte copies into aligned locals. Full signed WORD
indices, byte interpretation, signed Rune extension and destination publication
keep their existing semantics.

AMD64/ARM64 INSC, SLICEC and ADDC apply matching alignment guards before Heap,
header or payload loads. Packed owners decline without stores, allowing the
complete JIT entry to use its C handler and report the existing VM error.
Borrowed comparison, UTF conversion and character reads retain packed-view support. Emitters use
the existing code budget and ABI; no opcode is removed or made C-only.

The C driver passes 47,529 scenarios per scalar model:

- 39,168 packed character reads over both representations, 16 owner shifts,
  lengths 1 through 17, every position and eight native pointer-cell shifts;
- 2,176 split/copy/expansion scenarios over unique/shared sources, lengths 0
  through 16, eight pointer-cell shifts and deterministic allocation failures;
- 6,144 preflight rejections across character insertion, in-place and allocating
  slicing, concatenation and both split modes; complete source images, the
  caller cell, allocation counts and ownership remain unchanged;
- 41 pointer-cell bounds, H and null-owner scenarios.

Malformed expand-mode inputs are also tested with allocation failure armed;
exHeap must precede exNomem. Fourteen previous-source controls independently
fail the four owner helpers, expand-mode validation, character reads and pointer
cells in each scalar model. Existing nonexpanding stringdup guards are retained.

The instruction driver passes 9,216 cases per architecture. It varies both WORD
widths, ASCII/Rune storage, owner and operand-cell shifts, and both ADDC owners.
A read hook forbids any owner metadata/payload read on rejected cases. A write
hook and full memory-image oracle check rejection and successful operations.
Six previous-emitter controls independently expose premature reads for each
opcode/backend pair. STRING_OWNER_ARCHES and STRING_OWNER_OPERATIONS select
individual instruction controls; EMITTER_DIR selects the parent headers.

For C controls, STRING_SOURCE selects the source, STRING_VIEW_MODELS selects
native/word32, STRING_OWNER_ROUTE selects one of the six preflight routes and
STRING_OWNER_FAMILY selects reads or cells.

Adjacent checks cover C/RX string stores, slicing and append, existing machine
oracles and complete-entry publication. Availability remains 173/173 JIT+C and
166/173 guarded native paths for both AMD64 and ARM64. These counts describe
opcode dispatch and partial native operand domains. Full native VM ABI/layout
migration, boot acceptance and physical ARM64 execution remain separate.

## Reduced-address fixture follow-up

The older string-cell-spans-c and utf8-view-spans-c drivers narrow address
arithmetic to 16 bits to test overflow with real allocated backing storage.
Byte-safe indc/string2c now also use that address variable for payload access;
blindly narrowing it made those test fixtures dereference a truncated pointer.
The adapters retain every reduced-width guard and independent extent oracle,
but use the original native String data pointer for actual loads, termination
and returned UTF views. No production handler or guard is replaced or weakened.

Старые обвязки с 16-битной моделью адресной арифметики теперь сохраняют полный
указатель для реального доступа к данным. Все проверки переполнения и независимые
ожидания остаются; исправлена обвязка, а не обработчики интерпретатора.
