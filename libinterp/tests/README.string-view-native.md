# Packed string views and aligned duplicate owners

Run `python3 libinterp/tests/string-view-native.py` from the repository root.
The driver extracts production string2c, stringdup, stringcmp and constructors,
uses the real VM headers and lib9 Rune encoder, and runs a joined pthread with
ASan/UBSan and NDEBUG by default. All fixture predicates remain active. The
pthread gives ASan a tracked stack for the fixture's VM-error longjmp boundary.
LeakSanitizer process inspection is disabled because /proc is unavailable;
VM allocation guards and reference accounting remain active.

string2c reads lengths, capacity and the UTF cache pointer through byte copies.
Rune encoding copies each Rune into an aligned local value. Cache replacement
publishes through a byte copy and retains the previous allocation on failure.
ASCII views still get an inline terminator, including empty views. No new UTF
normalization or invalid-Rune policy is introduced.

stringcmp reads both headers before its identity shortcut and copies Rune keys
through aligned locals. It retains signed Rune ordering, unsigned ASCII ordering,
common-prefix-only bounds checks, empty/nil behavior and error precedence.
stringdup requires an aligned owning Heap prefix before reading its reference
count. It captures source geometry before allocation and copies exactly the
logical payload, without borrowing the source UTF cache. Packed borrowed views
are supported by comparison and UTF conversion; a packed Heap owner is rejected.
These arithmetic checks do not establish that arbitrary pointers are mapped.

Each WORD model passes 26,002 scenarios:

- 16 byte shifts, lengths 0 through 64 and ASCII/Rune representations, including
  independent expected UTF bytes, failed cache replacement and successful retry;
- 80 malformed packed metadata cases that leave payload bytes untouched;
- 2,080 owner/length/representation cases, checking aligned copies and rejecting
  packed owners before allocation;
- 20,736 comparison pairs covering every pair of eight byte shifts, all four
  representation combinations and nine keys through the full 32-bit Rune range;
- nil errors and H behavior, with balanced allocated references and payload guards.

WORD32 changes only WORD/UWORD in a temporary header; pointers stay native.
Six separate controls run the previous production source: UTF, duplicate-owner
and comparison families in both scalar models. Each reports a UBSan alignment
error. STRING_SOURCE selects that source, STRING_VIEW_MODELS selects native or
word32, and STRING_VIEW_OWNERS / STRING_VIEW_COMPARE_ONLY isolate the families.

Adjacent validation includes string-codecs.py (40,188 cases per model),
string-compare-spans-c.py (156,672 per model), the complete native branch fixture
and the AMD64/ARM64 instruction comparison oracle. Emitters and opcode coverage
are unchanged: each priority backend has 173/173 JIT+C entries and 166/173 guarded
native paths. These are operand-domain paths, not a completed 64-bit VM ABI;
compiler reference cells, frame layouts and a full boot remain separate work.
