# Experimental scalar selection for runtime builds

`DIS_WORD32=1` selects signed/unsigned 32-bit Dis words in the actual
`include/interp.h`. The header rejects other values and hosts without 8-bit
bytes and 32-bit int. With the macro absent, existing typedefs remain unchanged.
This build option is not a wire flag, ABI ID, extension or loader negotiation.

Every translation unit linked into one runtime must use the same selection.
Never reuse legacy objects when changing it: discard the entire object tree,
including generated interface consumers and native libraries. It is not safe
to link the two scalar models together. No default mkconfig target is changed.

`python3 libinterp/tests/word32-build.py` builds 18 complete core translation
units for both selections: dispatch/decoder, allocation/GC, loading, strings,
stack/unwind, and AMD64/ARM64/RV64 compiler sources. The native probe includes
actual interp.h and checks scalar sizes and existing frame-link offsets.
It does not link an emulator or execute emitted ARM64/RV64 machine code.
Compilation on one host does not certify a physical target.

Frame allocation, MFRAME, CALL/MCALL/RET, stack-unwind and Dis64 native pilot
fixtures now select the production WORD32 declaration instead of substituting
its typedefs. Their isolated runtime dependencies remain fixture dependencies.
They do not prove that builtins, generated module interfaces or GC maps are
ready for a full Dis64 program. Legacy-model coverage remains in the same tests.

The production runtime still needs a Dis64-aware loader, module allocation,
entry admission and actual IEXIT error/unwind/scheduler integration. Existing
Frame links matching the first 32 bytes of Frame64 do not establish argument,
result or temporary-slot compatibility. Existing GC maps cannot be silently
reinterpreted as map8. Calls remain disabled in the shared scalar pilot.

The next runtime integration must check ABI/profile and mandatory extensions
before allocation, use validated descriptors and initialized unpublished
storage, and establish scheduler-owned cleanup before entry. Do not enable a
new loader merely because these object files compile or a scalar RX leaf runs.
