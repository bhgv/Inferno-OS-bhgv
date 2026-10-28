# Fixed Sys status frames, descriptors and directory capture

Run `python3 libinterp/tests/sys-status-capture.py` from the repository root.
This test is included in the CORE checkpoint. It extracts both ports' actual
`dup`, `remove`, `seek`, `unmount`, `chdir`, `mount`, `bind`, `iounit`, `wstat`,
`fwstat`, `millisec`, `sleep`, `export`, the shared fixed-frame/descriptor validators and real
`packdir`. Generated frames and directory declarations remain authoritative.
Native namespace and scheduler operations are controlled observers.

Default ASan/UBSan/NDEBUG runs exercise these four profiles:

| Family | WORD64 per port | WORD32 per port | Both ports and models |
| --- | ---: | ---: | ---: |
| Ordinary capture | 199,680 | 199,680 | 798,720 |
| Descriptor boundaries | 184,320 | 92,160 | 552,960 |
| Scalar integer boundaries | 153,600 | 92,160 | 491,520 |
| Preflight and allocation failures | 92 | 92 | 368 |

The three ordinary families total **1,843,200** scenarios. All predicates are
explicit checks and remain active under `NDEBUG`.

Ordinary capture crosses 13 operations, six callback phases, eight frame
shifts, eight result shifts, eight FD-header shifts and five native result
values. The phases are none, release, string conversion, native call,
allocation, and acquire. A callback fires only at a boundary reached by the
selected operation. Callbacks overwrite the original frame and both
borrowed FD numbers. They must stay visible: wrappers must use captured
operands and must never restore the original frame. Full frame and result
images detect unintended stores; seek checks complete signed 64-bit offsets
and results, including their minimum and maximum values.

Descriptor cases cover six targets: seek FD, mount FD, mount authentication
FD, iounit FD, fwstat FD and export FD. Six valid cases include zero, negative one,
INT_MIN, INT_MAX and the H handle convention. Six additional WORD64 cases
check positive and negative numbers outside native int, including nonzero
upper bits whose lower bits resemble an ordinary FD. Rejection must publish
minus one without converting strings, allocating a directory, releasing the
VM or calling native code. Callbacks are enabled for valid cases.

Scalar cases independently vary dup's old and new descriptors, seek's start,
mount/bind flags and the sleep period. Six representable values and four WORD64-only values
cover signed boundaries and high bits. Invalid seek/mount numeric arguments
are accompanied by a nil FD to verify that native argument rejection retains
its priority over inspecting unused descriptor operands.

Preflight launches fresh processes for 78 invalid frame/result cases,
ten invalid main FD cases, two invalid mount authentication FD cases and
two directory-allocation failures per profile. It checks exact VM error
identities, untouched complete result images and absence of native side
effects. On allocation failure the real packer has converted exactly four
directory strings but has acquired no allocation or native descriptor.

The real directory packer must preserve all four strings, qid fields, mode,
timestamps, type, device and a length close to LLONG_MAX across callbacks.
The fixture accounts for its one allocation and verifies its exact cleanup
after successful and negative native status returns. The main test body runs
on a joined pthread to give ASan a tracked stack in restricted environments;
only unsupported LeakSanitizer process inspection is disabled.

`SYS_CAPTURE_SOURCE` selects an alternate port source. `SYS_CAPTURE_PORTS`
(default `emu os`), `SYS_CAPTURE_MODELS` (default `native word32`) and
`SYS_CAPTURE_FAMILIES` (default `ordinary nativefd nativeint preflight`)
select profiles. Ordinary selectors `SYS_CAPTURE_OP`, `SYS_CAPTURE_PHASE`,
`SYS_CAPTURE_FRAME`, `SYS_CAPTURE_FD`, `SYS_CAPTURE_RESULT` and boundary
selectors `SYS_CAPTURE_TARGET`, `SYS_CAPTURE_NUMBER` isolate regressions.
Native-FD phase defaults to release; phase zero isolates descriptor narrowing
from callback capture. Any selected ordinary family must execute at least
one scenario. `SYS_CAPTURE_PREFLIGHT` isolates one validated preflight key.

The earlier 11-operation checkpoint against parent `2bc34ef` was checked with 114 independent runtime failures:
packed frames and callback capture for each of 11 operations in both ports
and models (88 controls), conversion/allocation callbacks for both directory
wrappers in both ports and models (16 controls), and five wide-FD targets in
both ports under WORD64 (10 controls). Each control must compile and then fail
with a sanitizer diagnostic or active fixture predicate.

These tests do not boot Inferno, modify a namespace, or prove scheduler error
unwinding and the full compiler/loader/GC 64-bit ABI. WORD32 still uses native
host pointers. Packed FD headers are borrowed byte views without ownership
transfer. Clock reads are controlled observations rather than elapsed-time
measurements. Sys builtins are reached through native module calls; they are
not additional Dis opcodes or new native JIT fast paths.

Sleep adapters are controlled observers: positive waits check their captured
period, nonpositive waits return zero, and kernel waits return zero after
completion. This fixture does not inject nonlocal scheduler errors. See
[stream and directory capture](README.sys-stream-directory-capture.md) for
the newer stream/count and directory matrices and their parent controls.
