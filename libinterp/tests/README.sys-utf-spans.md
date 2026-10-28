# Complete Sys UTF frame, array and result spans

Run `python3 libinterp/tests/sys-utf-spans.py` from the repository root.
The driver extracts production Sys_utfbytes, Sys_byte2char, Sys_char2byte and
their private helpers, using real generated builtin frames, real VM headers
and lib9/rune.c. It runs a single joined pthread with ASan/UBSan and NDEBUG;
all new predicates remain active. The registered worker stack permits normal
VM-error longjmp instrumentation. LeakSanitizer process inspection is disabled
for the unavailable /proc; the new fixture uses bounded static storage.

Each public entry now checks its complete generated frame before copying it
into an aligned local frame. The result span is checked before array metadata
or UTF effects: one WORD for utfbytes/char2byte, three WORDs for byte2char.
Nil/H results retain exNilref; wrapped spans report exBounds. Output addresses
and arguments remain captured even when result storage overlaps input bytes.

sysutfarray bounds the owning prefix and complete Array header, reads borrowed
headers through byte copies, validates noncanonical pointer-free byte Type
prefixes before field reads, and checks the complete nonempty data extent.
Read-only arrays/descriptors may be packed. The validated data pointer and int
length are returned as a captured view; public entries no longer dereference
a packed Array directly. Invalid byte Types retain exType, while invalid array
lengths/data spans retain exBounds. Empty arrays need no data pointer.
These numeric span checks do not establish that arbitrary addresses are mapped.

The encoder/decoder semantics are unchanged: invalid Rune replacement, complete
and incomplete sequences, embedded NUL, offsets and count publication still
use the existing production UTF routines. Generated frame definitions, Dis
format and public builtin signatures are unchanged.

The new default suite passes 295,341 WORD64 and 295,265 WORD32 scenarios:

- 294,912 cases per model cover every combination of eight frame, Array,
  descriptor and result shifts, all three builtins and twelve independent UTF
  keys, with separate results and results overlapping input byte storage;
- rejected low/wrapped Array origins, incomplete Type prefixes, negative lengths,
  nil data, wrong-type short objects and wrapped byte payloads preserve the output and payload images;
- each incomplete native WORD/tuple result is rejected before a poisoned Array
  can be read;
- wrapped generated frames fail before any field access, including H frames.

WORD32 changes only WORD/UWORD in a temporary real header; pointers remain
native. Existing sys-utf.py additionally passes 1,115,061 scalar/offset/alignment/
malformed/bounds cases per model under UBSan. Its exhaustive Unicode oracle is
independent of the production Rune encoder.

Previous-source controls isolate packed accesses, generated frames, low/wrapped
Array headers, descriptor prefixes, data extents and result extents for every
builtin and both WORD models. They fail through sanitizer diagnostics or active
error/image predicates, rather than compiler/link failures.
SYS_UTF_SOURCE selects the previous runt.c, SYS_UTF_MODELS selects native/word32,
SYS_UTF_OP selects a builtin (0/1/2), SYS_UTF_FAMILY selects packed/errors/results/
frames, and SYS_UTF_ERROR selects an individual metadata rejection.

Adjacent checks include full C/RX dispatch, ARM64 compiler publication and native
MCALL tests. Sys UTF remains a C builtin invoked through the runtime; this series
does not add Dis-native opcode paths. Availability remains AMD64/ARM64 173/173
JIT+C entries and 166/173 guarded native paths. Full 64-bit reference/frame ABI
migration, VM boot and physical ARM64 execution remain separate acceptance work.
