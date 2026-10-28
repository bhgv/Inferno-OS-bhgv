# Tokenizer frame, delimiter and result preflight

Run `python3 libinterp/tests/token-spans.py` from the repository root. The driver
extracts production Sys_tokenize, buildtokens, tokdelim, string constructors,
slicer, cons and freeptrs. It uses actual emu/port and os/port kprotect/krethrow,
real VM declarations and the bounded allocation/reference oracle shared with
sys-tokenize.c. Every inherited predicate remains active with NDEBUG.

The default run enables ASan/UBSan on a joined pthread so nested VM error frames
have a registered stack. LeakSanitizer process inspection is disabled because
/proc is unavailable; counted VM heap blocks and descriptor references must
balance after every scenario.

Sys_tokenize now bounds the complete generated frame and return tuple, captures
its operands and old result through byte copies, and publishes only after the
protected builder succeeds. The old result Heap origin, alignment and live
reference are validated before allocation or destruction. Input metadata errors
retain precedence over invalid old owners; output-span errors precede any
construction. No result field or old reference is changed on rejection.

buildtokens checks both string prefixes, logical capacities and accessed full
payload spans before scanning or constructing words. An owning source has a
checked Heap prefix; delimiters remain borrowed and can be packed. Metadata
errors preserve exInval; invalid native spans/owners use exHeap. H source retains
its lazy empty-result behavior without inspecting unused delimiter metadata.

Delimiter headers and Rune characters are read as bytes into aligned locals.
ASCII characters remain unsigned and Rune matching retains its full bit pattern.
Source scanning and new list-head publication also use byte copies. Empty views
avoid forming unused payload pointers. The protected partial-list owner still
handles every cons/slicer allocation failure and rethrows the original VM error.
No allocation is added by validation and no public builtin/frame ABI is changed.

Each runtime/WORD combination passes 32,847 new cases (131,388 total):

- 32,768 construction/failure cases over every byte shift of the generated frame,
  return tuple and borrowed delimiter, both source/delimiter representations,
  empty or independently owned previous lists, and all six allocation failures;
- exact three-word results include an embedded NUL; source references, result
  padding/canaries, retained old results and all heap/type counts are checked;
- rejected result/frame/metadata spans, bad capacities and nil/low/dead old
  owners allocate nothing and preserve the complete caller result;
- every incomplete native return-tuple span is rejected before construction.

WORD32 changes only WORD/UWORD; pointers stay native. The arithmetic preflight
does not prove that arbitrary pointer values identify mapped allocations.
The fixture models VM allocation with real layouts and guards; full boot and
collector scheduling remain outside its scope.

Fourteen previous-source controls independently reproduce packed accesses,
return spans, delimiter spans, three old-owner failures and frame spans in both
WORD models. TOKENIZE_SOURCE selects the previous runt.c;
TOKEN_SPAN_RUNTIMES and TOKEN_SPAN_WORDS select error runtime/scalar profiles;
TOKEN_SPAN_FAMILY selects packed/errors and TOKEN_SPAN_ERROR isolates a rejection.

Adjacent sys-tokenize.py passes 218,458 cases per runtime/WORD combination;
sys-tokenize-matrix.py passes 8,322 per model; token-capacity.py and token-retains.py
retain 128 and 320 cases per runtime/model. MCALL C/RX, full dispatch and ARM64
compiler publication also pass. Sys.tokenize remains a C builtin. AMD64/ARM64
coverage stays 173/173 complete JIT+C entries and 166/173 guarded native opcode
paths; full 64-bit reference/frame migration and physical ARM64 boot remain
separate acceptance work.
