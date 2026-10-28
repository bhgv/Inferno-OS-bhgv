# Native file-query and creation capture

Run `python3 libinterp/tests/sys-query-capture.py` from the repository root.
The CORE checkpoint includes this driver. It runs actual emu/os implementations
of `stat`, `fstat`, `fd2path`, `open`, `create` and `fildes`, together with real
string constructors, `retstr`, directory unpacking, FD adoption/finalization,
and nested runtime protection frames. Native filesystem operations are bounded
observers rather than real namespace changes.

| Family | WORD64 per port | WORD32 per port | All four profiles |
| --- | ---: | ---: | ---: |
| Query callbacks and allocation failures | 365,568 | 365,568 | 1,462,272 |
| Query descriptor boundaries | 55,296 | 27,648 | 165,888 |
| Creation callbacks and allocation failures | 129,024 | 129,024 | 516,096 |
| Query preflight | 36 | 36 | 144 |
| Creation preflight | 30 | 30 | 120 |

The ordinary matrices total **2,144,256** scenarios, plus **264** explicit
preflight checks. Defaults enable ASan, UBSan and NDEBUG. Every fixture
predicate remains active under NDEBUG. Joined pthreads provide tracked stacks
without relying on process-stack discovery. Unsupported LeakSanitizer process
inspection is disabled; heap guards and explicit owner counts remain active.

Query capture crosses every original fixture scenario with eight frame shifts,
eight FD shifts and seven callback phases. The original scenarios already
cover eight result shifts, native failure/success, shared or distinct previous
strings, nil directory fields, and each successive conversion allocation
failure. Callback phases are none, release, native call, acquire, allocation,
string conversion and old-owner finalization. Only boundaries reached by the
selected operation fire. Callbacks overwrite the original frame and FD number;
complete frame images must retain those changes without state restoration.
Result images include canaries and generated tuple/record padding.

Query descriptor cases cover H, zero, negative one, INT_MIN, INT_MAX and a
normal positive FD. WORD64 also checks six signed values outside native int,
including high-bit values whose lower bits resemble a normal descriptor.
Invalid values must avoid native calls and conversions. Fstat leaves previous
record owners intact with status minus one. Fd2path releases its previous
string and publishes H, matching its native-failure result convention.

Creation capture crosses eight frame shifts and seven callback phases with
all file-creation fixture cases and native-width argument profiles. It uses
real `mkfd` and `freeFD`, detects native leaks/double closes including FD zero,
checks file-group and type references, and injects each allocation failure.
The original result cell must be cleared before finalizing its old FD. Both
normal completion and error cleanup preserve callback changes to the caller
frame. Wide open/create modes must produce an ordinary argument error after
releasing the old result, without creating a native descriptor. Creation
permissions retain the existing native `ulong` contract.

Query preflight checks nil/H/wrapping fixed frames, nil/H/wrapping complete
result spans, nil/low/packed/dead previous owners, insufficient references for
duplicate directory owner slots, and nil/wrapping borrowed FD headers. The
wrapping stat result permits its leading status word while rejecting the full
record extent. All preflight errors preserve every result byte and previous
owner before native entry or allocation. Creation preflight repeats the frame,
result and individual owner checks with an actual previous FD, verifying its
raw descriptor and file-group reference remain live until fixture cleanup.

The descriptor-header checks are borrowed byte reads. Previous result owners
must have a valid aligned heap prefix and a nonzero reference count. Directory
owner preflight additionally counts repeated pointers, so replacing four slots
cannot consume references that the owner does not hold. These checks do not
prove arbitrary host addresses are mapped or validate every heap type tag.

`SYS_QUERY_FAMILIES` selects `query`, `nativefd`, `query-preflight`, `creation`
and/or `creation-preflight`. Underlying fixture controls remain available:
`SYS_STAT_SOURCE`, `SYS_STAT_PORTS`, `SYS_STAT_MODELS`, `SYS_STAT_OPERATION`,
`SYS_STAT_PHASE`, `SYS_STAT_FRAME`, `SYS_STAT_FD_SHIFT`, `SYS_STAT_ERROR`,
`SYS_STAT_NUMBER`; and `FD_OWNER_SOURCE`, `FD_OWNER_PORTS`, `FD_OWNER_MODELS`,
`FD_OWNER_OPERATION`, `FD_OWNER_PHASE`, `FD_OWNER_FRAME`, `FD_OWNER_ARGUMENT`,
`FD_OWNER_ERROR`. Model selectors use `64 32`. Native-FD phase zero isolates
integer narrowing from callback capture. The default legacy matrices remain
available through `sys-stat-slots.py` and `sys-fd-owners.py`.

Against parent `db39f42`, **200 separately compiled controls fail at runtime**:
52 query frame/FD/callback controls, 44 query owner-preflight controls,
60 creation frame/callback controls, 36 creation owner-preflight controls,
four wide query FD controls and four wide creation-mode controls. Each must
produce a sanitizer diagnostic or active fixture predicate, not a compilation
failure. Nearby regression runs also cover the complete FD owner suite,
scalar Sys capture, byte I/O, MCALL and the x64/ARM64 compiler bridges.

These tests do not boot an emulator or kernel, use a real filesystem, or prove
the complete compiler/loader/GC 64-bit ABI. WORD32 retains host pointers.
Sys builtins are C module operations reached through MCALL, not additional Dis
opcodes; this series does not add new native opcode fast paths.

For pipe/dial/announce/listen and captured aggregate owners, see
[captured pipe and connection ownership](README.sys-multi-capture.md).
