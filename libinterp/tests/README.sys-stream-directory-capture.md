# Stream, wait, export and directory capture

Run `python3 libinterp/tests/sys-stream-directory-capture.py` from the repository
root. The CORE checkpoint includes this driver and the expanded
`sys-status-capture.py` matrix for sleep/export. Both ports' actual C builtin
bodies are extracted with generated frames and native shared declarations.

| Family | WORD64 per port | WORD32 per port | Four profiles |
| --- | ---: | ---: | ---: |
| Stream callbacks and oversized reports | 81,920 | 81,920 | 327,680 |
| Stream FD boundaries | 12,288 | 6,144 | 36,864 |
| Directory callbacks and partial failures | 279,552 | 279,552 | 1,118,208 |
| Directory FD boundaries | 147,456 | 73,728 | 442,368 |
| Directory native count boundaries | 21,504 | 21,504 | 86,016 |
| Fixed Sys capture, now including sleep/export | 537,600 | 384,000 | 1,843,200 |

These matrices total **3,854,336** scenarios, excluding repeated layout checks.
Additional preflight errors total **456**: 40 stream, 48 directory and 368 fixed
Sys cases. The legacy directory fixture prints one additional layout check per
profile; that layout predicate is also executed for its extended families.
Legacy stream and full-range directory tests remain separately runnable.

All drivers default to ASan, UBSan and NDEBUG. Predicates remain active with
NDEBUG; copied shared directory-prefix predicates are converted to explicit
checks in generated test units. Joined pthreads provide sanitizer-tracked
stacks while preserving real VM protection frames for directory allocation
failure and cleanup. Unsupported LeakSanitizer process inspection is disabled;
heap guards, reference/type counts and native-buffer accounting remain active.

Stream capture crosses eight frame shifts, eight FD shifts, eight result
shifts, eight callback phases and twenty transfer scenarios. The phases are
none, allocation, release, read, write, acquire, free and running-program
lookup. Callbacks replace the original frame and both FD numbers. Full frame
images must retain those changes, and the original result cell must receive
the byte count. Tests compare exact acknowledged output prefixes and canaries
under short reads/writes, early and late errors, cancellation, allocation
failure and oversized native reports.

Native reads and writes return long. Before using those values as int-sized
buffer offsets or counts, the stream wrapper/helper must reject values above
the requested span. Cases report requested+1, 2^32+requested and LONG_MAX,
both on the first transfer and after progress. Oversized reports contribute
no acknowledged bytes; earlier valid bytes are retained. No large native
memory access is performed. Existing virtual multi-gigabyte tests separately
preserve modular VM-width byte-count behavior without allocating gigabytes.

Stream FD cases independently vary source and destination over six valid
values/conventions, plus six WORD64-only values outside signed native int.
Invalid numbers must return minus one before buffer allocation, release or
native I/O. Ten preflight cases per profile check nil/H/wrapping frames and
results plus nil/wrapping source/destination FD headers without result writes.

Directory callback cases retain the original exhaustive owner/failure oracle,
restricted to native counts -1 through 3 while crossing seven callback phases
and all frame/FD/result byte shifts. The separately retained legacy matrix
checks counts through 16. The actual heaparray, unpackdir, string constructors,
retstr and mapped finalizers build and release every partial result. Tests
check independent record fields, native record strides, all string ownership,
full tuple images and padding, type references, heap guards and native frees.
Callback changes must survive even when conversion fails partway through.

Directory FD cases include H, zero, negative one, native signed boundaries and
high-bit WORD64 values. Native count cases use a small owned native allocation
while reporting six long values outside signed native int, including values
whose low bits resemble zero or one record. Every such report must keep the
result (-1,H), record an argument error and free the native allocation before
any new VM allocation. The result array's old owner is finalized only after
its validated original tuple is cleared. Initializing the native entries
pointer also makes ordinary native failure safe when it leaves that output
unchanged. Native entries are otherwise trusted to follow their C API contract.

Directory preflight checks twelve cases per profile: invalid frames, complete
tuples, previous owner origins/alignment/reference counts and FD header spans.
The wrapping tuple permits its leading status word but lacks the array field;
preflight must reject it before publication or destruction. Previous owners
and every result byte remain unchanged on VM preflight errors.

The expanded fixed Sys fixture covers thirteen operations. Sleep captures its
period and original result before release and acquire callbacks. Nonpositive
periods return zero; positive kernel waits return zero after completion, while
host waits preserve their native result. Export captures its directory, masked
flags, FD and result, checking descriptor width before releasing the VM. Its
pre-existing async flag mask is preserved. Native wait/export operations are
controlled observers; this fixture does not inject scheduler nonlocal errors.

`STREAM_DIRECTORY_FAMILIES` selects the six driver families listed in its
source. Underlying stream selectors include `SYS_STREAM_SOURCE`,
`SYS_STREAM_PORTS`, `SYS_STREAM_MODELS`, `SYS_STREAM_CASE`, `STREAM_PHASE`,
`STREAM_FRAME`, `STREAM_FD_SHIFT`, `STREAM_MODE`, `STREAM_TARGET` and
`STREAM_NUMBER`. Directory selectors include `DIRREAD_SOURCE`, `DIRREAD_PORTS`,
`DIRREAD_MODELS`, `DIRREAD_CAPTURE`, `DIRREAD_FAMILY`, `DIRREAD_PHASE`,
`DIRREAD_FRAME`, `DIRREAD_FD_SHIFT`, `DIRREAD_ERROR`, `DIRREAD_NUMBER` and
`DIRREAD_REPORT`. Model selectors use `32 64`. Fixed Sys preflight keys can be
isolated with `SYS_CAPTURE_PREFLIGHT`.

Against parent `de42fc4`, **216 independently compiled controls fail at
runtime**: 80 stream controls, 82 directory controls (including 24 native count
controls), and 54 sleep/export controls. They cover packed frames and FD views,
callback capture, native over-reports and narrowing, plus complete frame,
result and owner preflight. A sanitizer diagnostic or active fixture predicate
must establish each failure; compilation errors do not qualify.

These tests do not boot Inferno, run physical ARM64 hardware or prove the
complete compiler/loader/GC 64-bit ABI. WORD32 retains host pointers. Sys C
builtins reached through MCALL are not additional Dis opcodes. Complete JIT
entry coverage and native opcode fast-path counts are unchanged by this series.
