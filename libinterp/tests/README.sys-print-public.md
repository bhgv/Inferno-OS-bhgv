# Public formatted-result frame and ownership capture

Run `python3 libinterp/tests/sys-print-public.py` from the repository root.
The shared formatter driver extracts production Sys_sprint/Sys_aprint, xprint,
bigxprint and protected result construction, links real lib9 formatters and
uses actual generated frame/VM declarations and emulator error frames.
ASan/UBSan and NDEBUG are enabled by default; predicates remain active on the
shared joined pthread stack.

Both public wrappers now bound the generated fixed frame prefix and complete
native result cell before formatting or allocation. They capture the return
address, previous owner, format and original packed argument cursor with byte
copies. Nil/H cells retain exNilref; incomplete frame/result spans use exBounds;
invalid owning Heap origins/alignment/zero references use exHeap before any
output side effects. Generic live old owners remain eligible for release.

The original frame remains the vararg alignment anchor, including during
bigxprint retries. Copying the fixed metadata into locals does not relocate
arguments or change the historical classic vararg cursor widths/alignment.
The output is published to the original caller cell after construction, even
if an allocation callback changes the original frame's ret/format fields.
Callback changes remain visible; no whole-frame restoration is introduced.
Exactly one previous reference is released after successful publication.
Formatting or conversion failures preserve the original result and release
dynamic formatting storage through the existing protected cleanup.

The shared fixture's constructor boundary now provides real live Heap prefixes
and refcounts, with independently counted VM/output allocations. Its bounded
String/Array constructors and destruction remain fixture implementations;
the optional VM allocation hook models a heapmonitor callback borrowing the
original frame. Production formatting and protected result helpers are not
replaced. This is not a full VM heap/collector or boot integration test.

Each scalar model passes 16,142 new scenarios:

- 15,360 valid/failing constructor cases over both builtins, every byte shift
  of frame and result cell, ASCII/Rune format storage, short/grown output,
  classic int/64-bit big arguments and embedded NUL output;
- allocation callbacks off/on, constructor failure off/on, and H/unique/shared
  previous owners, with exact output images, preserved callback frame changes,
  original-cell publication, balanced refcounts and zero leaked buffers;
- 768 rejected frame/cell/old-owner cases and fourteen final-address result-cell
  probes, before VM/output allocation or result mutation.

WORD32 changes WORD/UWORD only; pointers remain native. Numeric guards do not
prove arbitrary pointer values refer to mapped allocations. The active classic
vararg layout is retained; this does not complete the future native-reference
ABI migration.

Thirty-two previous-source controls independently fail original-cell callback
publication, packed frame access and six preflight families, for both public
functions and both scalar models. SYS_PRINT_SOURCE selects the prior runt.c;
SYS_PRINT_MODELS selects native/word32; SYS_PRINT_CASE selects public/preflight;
PRINT_PUBLIC_OP, PRINT_PUBLIC_BORROW and PRINT_PUBLIC_ERROR isolate controls.

Adjacent sys-print.py keeps 2,028 original formatting/error/embedded-NUL cases;
sys-print-spans.py keeps 39,128 span/metadata/cache/error cases. Native MCALL,
complete C/RX dispatch and ARM64 compiler publication also pass. The wrappers
remain C builtins. AMD64/ARM64 availability remains 173/173 JIT+C entries and
166/173 guarded native paths; full reference/frame ABI migration and physical
ARM64 execution remain separate acceptance work.
