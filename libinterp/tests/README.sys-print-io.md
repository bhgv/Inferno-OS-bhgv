# Sys output snapshots and native descriptor bounds

Run `python3 libinterp/tests/sys-print-io.py` from the repository root.
The driver extracts the actual Sys_print, Sys_fprint and Sys_werrstr bodies
from both emu/port/inferno.c and os/port/inferno.c, links the production xprint/
bigxprint and real lib9 formatting engine, and uses each port's kprotect/krethrow.
Controlled release/acquire, write and error-copy boundaries replace external
I/O; no files, namespace or scheduler are modified by the fixture.

All six wrappers now validate their fixed generated frame prefix and complete
WORD return cell before output, conversion or VM release. Frame pointer fields
are copied as bytes so packed frames/results are supported. Print/fprint capture
the original format, packed argument cursor and return address while retaining
the original frame as the vararg alignment anchor. Callback changes to frame
fields remain visible without redirecting the operation or result publication.

Fprint captures a complete possibly packed Sys_FD.fd WORD before VM release,
checks it fits native signed int, and converts only after that check. High bits
cannot silently select another native descriptor. Invalid widths return -1 and
set Ebadarg before release, formatting or writing. H descriptors retain their
existing native -1 behavior; a C-nil descriptor raises exNilref. Representable
positive and negative native descriptor values retain their native handling.

Werrstr captures both the scalar return address and current error-buffer pointer
before string conversion. Allocation/native callbacks cannot redirect the error
text or final zero result by changing the frame/environment fields. Missing
stdout still returns -1 without formatting or release. Normal write results,
including errors, retain their complete signed WORD status and release/acquire
balance. Public frame definitions and classic vararg layout are unchanged.

New tests pass 484,992 ASan/UBSan/NDEBUG scenarios across both ports and scalar
models, on the shared joined pthread stack:

- 99,840 per runtime/WORD profile cover every byte shift of generated frame,
  result and FD, short/grown output, missing stdout/H descriptors, callback
  boundaries and signed native statuses; classic int and 64-bit big varargs
  exercise original-frame alignment and the separate fprint field layout;
- preflight has 960 WORD64 / 896 WORD32 cases per port, rejecting frame/result/
  descriptor spans before a poison format, native I/O or any release;
- descriptor arithmetic has 28,160 WORD64 / 12,800 WORD32 cases per port, with
  native signed-int endpoints and six wider signed WORD64 values. Wider values
  cause no formatting, write or callback, and report -1/Ebadarg;
- complete surrounding-byte images, preserved callback frame changes, unchanged
  alternate result/error buffers, dynamic-buffer ownership, full result width
  and exact release/acquire/write/copy counts are independently checked.

WORD32 changes only WORD/UWORD; pointers remain native. Numeric guards do not
prove arbitrary addresses identify mapped allocations. The fixture controls
native side effects and the string-conversion boundary; it does not boot either
kernel or run a complete scheduler/collector. Actual host emu/port/inferno.c also
passes strict syntax checks with implicit declarations and pointer-type mismatch
warnings treated as errors.

Eighty-two previous-source controls separately expose callback publication,
packed accesses, four call/result families, two FD pointer failures and native
FD narrowing in both ports/models. SYS_PRINT_IO_SOURCE selects a prior inferno.c;
PRINT_IO_PORTS selects emu/os; SYS_PRINT_MODELS selects native/word32;
SYS_PRINT_CASE selects io/preflight/nativefd; PRINT_IO_OP and PRINT_IO_ERROR
isolate wrappers/failures. PRINT_IO_NO_BORROW disables callbacks for independent
packing and descriptor arithmetic controls.

The inherited sys-status-slots fixture now retains its predicates with NDEBUG
and passes 21,120 cases. Original formatter, span and public-result suites retain
2,028, 39,128 and 32,284 cases. MCALL C/RX, complete dispatch and ARM64 compiler
publication also pass. Sys output remains in C; availability stays AMD64/ARM64
173/173 complete JIT+C entries and 166/173 guarded native opcode paths. Full
64-bit reference/frame migration and physical ARM64 boot remain separate work.
