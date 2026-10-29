# Shared checked fetch for instrumented executors

The normal `xec`, emu/os debugger and coverage executors, and OS benchmark now
share `include/vmfetch.h`. It checks the process, quantum, module/program span,
relative PC, opcode and callback entries before use. Opcode/addressing identity
is captured before decoding can overwrite REG aliases. Callback yields do not
decrement zero, negative values or INT_MIN. Native-PC transitions delegate to
`comvec`; diagnostic breakpoint inspection does not fetch a yielded one-past PC.
Instrumented bytecode loops retain C handlers so debugger and profiling events
remain visible. Build dependencies include the shared header and error/ISA inputs.

Debugger LOAD notification captures the original decoded source slot and copies
its pointer without an alignment assumption after execution. SPAWN/MSPAWN check
the child before installing hooks. Coverage looks up its record each instruction
instead of retaining a record across callbacks that can reset profiling.

Benchmark reset detaches ownership roots before freeing records, preserving new
records created by reentrant callbacks. Allocation failure releases private name,
path and instruction storage before publication; instruction counts are checked
before multiplication. Samples check record capacity and a 64-bit reset generation
after execution. Native timing hooks retain their existing native-ulong cells,
copy unaligned inputs, reacquire records and discard samples invalidated by reset.
Opcode names use explicit key values; the canonical table already has ISA order,
but reordered or shorter terminated tables now work too. Statistical arithmetic
and the native hook ABI are unchanged.

## Verification

```
python3 libinterp/tests/instrument-loops.py
python3 libinterp/tests/xec-fetch-native.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
python3 utils/dis2c/tests/test.py
python3 libinterp/tests/riscv-runtime.py
python3 utils/tests/native-bootstrap.py
```

The new fixture executes the actual five device function bodies and production
decoder under ASan/UBSan with NDEBUG, for WORD32 and WORD64 on the native host.
Each model passes 2,220 debugger predicates per variant, 2,516 coverage predicates
per variant and 3,532 benchmark predicates: **26,008 parent predicates total**.
Forked error-path children add checks outside that counter. Matrices cover packed
PC/data offsets, opcode/REG aliasing, callback yields, native handoff, debugger
step/stop/breakpoints/blocking, LOAD and child notifications, bounds failures,
profile/benchmark reset, failed allocation, cached-record capacity, metadata
permutations, compiled timing and reentrant record creation during reset.

Twenty-three selected parent-source controls fail during execution, including
both debugger and coverage variants and benchmark bounds/yield/native/reset,
lookup/name/compiled/reentry controls. The parent sources are the device files
at 43bdf6d. Replay with `INSTRUMENT_SOURCE=/path/to/parent.c`,
`INSTRUMENT_KIND=emu-debug|os-debug|emu-profile|os-profile|bench`,
`INSTRUMENT_WORD_BITS=32`, and `INSTRUMENT_CASE=<case>`. Parent benchmark replay
shares current private declarations and the unchanged statistical helper; it is
not a complete historical kernel build. Canonical opcode-name ordering passes
on the parent; metadata permutation and shorter-table cases expose the issue.

The fixture supplies controlled scheduler, controller, allocation and error
adapters, not a complete kernel. Error children use an indirect `_Exit` after
nested error unwinding to avoid the sandbox libasan abandoned-stack cleanup bug;
production and fixture stack instrumentation remain enabled. Both complete emu
device sources also pass native syntax checking. The normal fetch matrix retains
485,124 parent predicates; C/RX routing, ARM64 compiler preflight, AOT integration,
RV64 runtime adapter and all 17 native-bootstrap Limbo units pass.

Availability is unchanged: AMD64 and ARM64 each have 173/173 complete JIT+C
entries and 166/173 guarded native fast paths. These checks do not establish a
bootable 64-bit VM or execution on physical ARM64. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are unchanged.
