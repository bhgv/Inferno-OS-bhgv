# Captured pipe and connection ownership

Run `python3 libinterp/tests/sys-multi-capture.py` from the repository root.
The CORE checkpoint includes this driver. It extracts actual emu/os `pipe`,
`dial`, `announce`, `listen`, connection construction/clear, raw descriptor
transfer/cleanup, VM protection, string conversion and FD finalizers. Native
filesystem/network operations remain bounded observers, without real pipes or
network connections.

| Family | Per port and WORD model | All four profiles |
| --- | ---: | ---: |
| Pipe frame/array capture and allocation failures | 229,376 | 917,504 |
| Connection capture and allocation failures | 46,592 | 186,368 |
| Pipe array-only callback | 512 | 2,048 |
| Multi-owner preflight | 58 | 232 |

The ordinary capture families total **1,105,920** scenarios. Each default run
uses ASan, UBSan and NDEBUG, with all explicit predicates active. Joined
pthreads retain real nested VM protection frames while giving ASan a tracked
stack. Unsupported LeakSanitizer process inspection is disabled; heap guards,
file-group/type references and raw descriptor accounting remain active.

Pipe capture crosses seven callback phases, eight frame shifts, eight borrowed
array-header shifts, eight result/data shifts, native success/failure, shared
or distinct previous handles, new descriptor zero/positive values, two usable
array lengths and every allocation failure. The original full legacy matrix
separately retains all nine invalid/short/wrapping array scenarios. Callbacks
can replace both the original frame and the array's length/data fields. The
wrapper must publish to the originally captured slots and preserve callback
changes to the complete frame and borrowed array image. Extra array slots
remain untouched. The array-only family disables frame mutation to isolate
capturing the original data pointer.

Connection capture crosses the same reached callback phases and all frame and
result shifts with native failures, distinct/shared old handles, descriptor
zero, and each failure while adopting handles or converting the returned UTF-8
directory. Listen borrows its previous result's directory as input. Both the
complete fixed frame and previous connection must be captured before string
conversion, native entry or allocation callbacks. Helpers consume the captured
previous connection rather than refetching its owners after construction.

Callbacks occur at VM allocation, old-owner finalization, release, acquire,
native operation and input string conversion; phase zero has no callback.
Only boundaries reached by the selected operation fire. Tests compare complete
frame, pipe header, result and padding images. Every completed or failed case
must balance heap blocks, native descriptor closes, file-group/type references
and VM protection frames. Close observers deliberately overwrite errstr, so
cleanup must retain the native or allocation error that triggered it.

Preflight checks thirteen pipe cases and fifteen cases per connection wrapper
in each profile. Invalid nil/H/wrapping frames and results, nil/low/packed/dead
previous owners and insufficient references for repeated owner pointers must
leave every result byte and all previous owners unchanged. Directory owner
checks cover its nil, low, packed and zero-reference forms. Pipe additionally
rejects wrapping and obviously low borrowed array headers before publication.
Poisoned unused input strings ensure result-owner validation precedes string
conversion and native side effects. Exact VM error identities are checked.

The private multi-owner validator counts repeated pointers before replacement:
a shared FD needs enough references for both fields that will release it.
Its inputs are captured pointer slots, so packed VM result addresses do not
cause typed unaligned reads. Pipe consumes only its first two slots; nil/H,
short arrays and absent/wrapping data spans retain the ordinary minus-one
result. Header views are borrowed; this series does not add array element-type
verification, ownership transfer for borrowed headers, or proof that arbitrary
host addresses are mapped.

`SYS_MULTI_FAMILIES` selects `pipe`, `connection`, `preflight` and/or `array`.
Underlying controls include `FD_OWNER_SOURCE`, `FD_OWNER_PORTS`,
`FD_OWNER_MODELS` (`32 64`), `FD_OWNER_OPERATION` (4 through 7),
`FD_OWNER_PHASE`, `FD_OWNER_FRAME`, `FD_OWNER_ARRAY_SHIFT`, `FD_OWNER_ERROR`
and `FD_OWNER_KEEP_FRAME`. `FD_OWNER_MULTI` expands the capture matrix;
`FD_OWNER_MULTI_PREFLIGHT` selects metadata/owner errors. The legacy fixture
without these flags still runs its full adoption/files/pipe/connection matrix.

Against parent `2bf3a69`, **264 independently compiled controls fail at
runtime**: packed frames for all four wrappers, packed pipe headers, callbacks
at every relevant construction/finalization/native boundary, array-only
callbacks, and complete frame/result/previous-owner preflight. A sanitizer
diagnostic or active fixture predicate must establish each failure; compilation
errors do not qualify.

These are host C builtin tests, not a full emulator/kernel build or a physical
ARM64 execution. WORD32 retains native host pointers. MCALL and the complete
x64/ARM64 compiler bridges are checked separately. This series does not change
native opcode fast-path counts or complete the compiler/loader/GC 64-bit ABI.
