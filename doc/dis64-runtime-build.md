# Experimental scalar selection for runtime builds

`DIS_WORD32=1` selects signed/unsigned 32-bit Dis words in the actual
`include/interp.h`. The header rejects other values and hosts without 8-bit
bytes and 32-bit int. With the macro absent, existing typedefs remain unchanged.
This build option is not a wire flag, ABI ID, extension or loader negotiation.

Every translation unit linked into one runtime must use the same selection.
Never reuse legacy objects when changing it: discard the entire object tree,
including generated interface consumers and native libraries. It is not safe
to link the two scalar models together. No default mkconfig target is changed.

`python3 libinterp/tests/word32-build.py` builds 20 complete core translation
units for legacy, WORD32 and WORD32 plus opt-in file routing (60 objects): dispatch/decoder, allocation/GC, loading, strings,
stack/unwind, AMD64/ARM64/RV64 compiler sources, and the full emulator scheduler. The native probe includes
actual interp.h and checks scalar sizes and existing frame-link offsets.
It does not link an emulator or execute emitted ARM64/RV64 machine code.
Compilation on one host does not certify a physical target.

Frame allocation, MFRAME, CALL/MCALL/RET, stack-unwind and Dis64 native pilot
fixtures now select the production WORD32 declaration instead of substituting
its typedefs. Their isolated runtime dependencies remain fixture dependencies.
They do not prove that builtins, generated module interfaces or GC maps are
ready for a full Dis64 program. Legacy-model coverage remains in the same tests.

`prepare64pilot(path, image, bytes, jit)` now constructs a real unpublished
`Module` using `load64.c`, the shared semantic validator, runtime `dtype`/`heapz`,
and the actual optional leaf compiler. The caller owns the returned reference
and must unload it or transfer it to startup. Failure returns nil after protected
cleanup. Construction never inserts the module in the global module cache.
The API consumes neither the input bytes nor a caller module reference.
The immutable image and caller label must stay valid throughout the call.

This construction API remains separate from readmod/parsemod. The
[opt-in file-loader path](dis64-fileloader.md) now selects it when
DIS64_PILOT_LOADER=1; default format selection and existing file policy remain
unchanged. Emulator startup/boot still needs separate acceptance. `jit=0/1` requests C or the selected native leaf compiler;
a failed requested compiler causes construction failure, with all owners released.
`compiled` retains its existing meaning and stays zero for leaf JIT modules.

## Runtime admission and native representation

The accepted wire format remains unchanged. This runtime adapter has additional
implementation limits checked before allocation:

| Property | Current runtime admission |
|---|---|
| Native scalar/pointer model | WORD/UWORD32, LONG64, IEEE binary64, pointer64, native frame links0/8/16/24; Adr at least8 bytes |
| Image / object / instruction resources | At most8MiB /1MiB /65536 instructions |
| Module display name | Nonempty ASCII1..127, at most255 runes; NUL rejected |
| Managed references and callable interfaces | Rejected by the existing scalar pilot |
| Explicit middle immediate | Signed16, only a WORD-sized middle operand |
| Explicit MP/FP middle offset | Unsigned16, with the existing scalar bounds/alignment checks |
| LONG source immediate | Signed30 widened to a complete native LONG in Adr storage |
| Branch destination | Checked Dis-PC converted to a pointer within the new Inst array |

These are implementation restrictions, not new ABI flags or smaller wire ranges.
A valid general Dis64 image may therefore be rejected by this experimental API.
In particular, do not silently truncate signed30 middle operands into Inst.reg,
and do not expose a WORD-sized R.t to a LONG operation. Big shift counts remain
WORD-sized source operands; the shifted middle value is LONG-sized.

Only MP and entry-frame types are needed by this subset. They become native
type slots0 and1 respectively, independent of their source ordinal IDs. Type-index
operations/calls are not accepted, and no runtime callable identity is inferred
from this remapping. Both native descriptors have np0, since their validated maps
contain no references. No map8 GC/reference execution is claimed.

## Connected execution evidence and remaining integration

`python3 libinterp/tests/dis64-runtime.py` combines the actual constructor with
unchanged production bodies for descriptor allocation, heap initialization,
reference-count cleanup, root-stack allocation, VM fetch/decoder/xec, scalar
handlers, complete C/JIT dispatch, IEXIT, kprotect and destroystack. It tests all43
opcodes in the current scalar whitelist, wide/negative immediates, branch relocation,
MP/FP offsets, integer boundary arithmetic, checked shifts, divide errors, binary64
bit preservation, empty data, truncation, limits and each tracked metadata/pool
allocation failure. Executable-mapping allocation failures are not injected.
Guarded pool/OS boundaries remain fixture implementations; a live collector and
newprog/run-queue/progexit lifecycle are not executed. Native execution is Linux/AMD64.

Both hand-written golden images reach actual IEXIT->error("") through the real
VM loop and then actual stack/module cleanup in this test. That is stronger than
the earlier single-leaf pilots; it is still not a full emulator Dis64 run.
The original dis64-pilot-native.py intentionally remains a separate leaf-level test.

The newer [scheduler integration fixture](dis64-scheduler.md) also runs actual
schedmod/newprog and terminal progexit/delprog, including PID/run-queue removal
and retained instance-MP results. Its host worker, pool and OS resource boundaries
remain controlled fixtures. Dis64 readmod routing is available only under the explicit experimental build
selection; a complete emulator boot remains unverified. Existing Frame links matching the first32 bytes of Frame64
do not establish argument/result/temporary-slot compatibility. Calls, dependency
resolution, reference signatures and map8 GC remain separate acceptance gates.
