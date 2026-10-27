# LOAD, builtin returns and C-only spawn transitions

Complex LOAD/SPAWN/MSPAWN work remains in shared C, reachable through complete
AMD64/ARM64 JIT tail entries. Bytecode MCALL keeps its guarded native fast path;
builtin calls decline into C. No new opcode availability is claimed by counting
additional checks.

## Captured LOAD publication

LOAD validates complete source/index/result cells and the current module prefix
before conversion, filesystem or linker callbacks. The original result address
and source MP are captured before those callbacks. Result publication and old
owner release use that captured address even if a callback changes REG.d. A
`$self` load retains data from the original module rather than a replacement
REG.M. Existing ownership semantics remain: linkmod consumes the acquired
Module reference on failure; MP is retained/marked only after successful self
linking; the new result is published before the previous owner is released.

Import table entries are copied as native pointers at arbitrary byte alignment.
Every entry address and complete pointer-cell extent is checked before access,
avoiding typed unaligned loads and indexed pointer wrap. Self MP headers are
checked before backward header arithmetic or typed reference-count access.
These are numeric-span and native-layout checks, not proof that an arbitrary
fabricated address belongs to a mapped allocation.

## Builtin MCALL cleanup

A C builtin may change live FP. Cleanup now restores SP from the captured callee
frame, then restores caller FP from the saved links. The previous implementation
used the callback's changed REG.FP as SP and could lose the original stack cursor.

Returned Type.np is copied without alignment assumptions. Nil-sentinel,
wrapping-prefix and negative-map descriptors fail before decrementing the callee
owner or publishing caller context. A callback leaving a zero callee reference
count is rejected instead of wrapping an unsigned decrement. Callback side
effects are not rolled back; error tests verify the callee context and held
owner at this boundary. The suite does not claim to validate every possible
mutation of a Type or an active frame by arbitrary C code.

## Spawn preflight

SPAWN/MSPAWN bound target/index cells before reads and reject nil/H targets.
Bytecode targets must fit a full Inst span; legacy compiled native-PC tokens
retain their original interpretation. The tests check token propagation through
construction without executing a fabricated address. Both operations
validate native module prefixes before dereferencing them. MSPAWN also
rejects the H program sentinel. Invalid inputs must fail before child-stack
allocation, reference acquisition or scheduler publication. Existing positive
startup/spawn/alignment and allocation-failure scenarios remain passing.

## Verification

```sh
python3 libinterp/tests/module-transition-native.py
python3 libinterp/tests/spawn-preflight.py
python3 libinterp/tests/module-imports.py
python3 libinterp/tests/control-transfer-native.py
python3 libinterp/tests/call-return-native.py
python3 libinterp/tests/module-types.py
python3 libinterp/tests/stack-startup.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/control-transfer-jit.py
python3 libinterp/tests/jit-dispatch-jit.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-dispatch-coverage.py
```

New native drivers default to ASan/UBSan with NDEBUG and explicit predicates.
Actual VM headers, production handlers, compiler and executable complete tails
are used. LOAD conversion/reader/linker/destructor and builtin functions are
injected boundaries that preserve borrowed object lifetimes; owner counters,
publication order, MP marking and complete returned REG/frame images are checked.
This is not a complete filesystem/linker/GC integration run.

| New matrix | WORD32 | WORD64 | Total |
| --- | ---: | ---: | ---: |
| LOAD callback/alias/ownership C and RX routes | 86016 | 86016 | 172032 |
| LOAD operand/table/MP span rejections | 218 | 226 | 444 |
| Builtin MCALL C/fast-decline/complete-RX routes | 196608 | 196608 | 393216 |
| Builtin invalid return-descriptor/reference errors | 560 | 560 | 1120 |
| SPAWN/MSPAWN C/RX pre-publication errors, emu + os | 1728 | 1744 | 3472 |
| Legacy native-PC construction checks, emu + os | 8 | 8 | 16 |

LOAD covers self/external/data-free paths, all operand alignments, unaligned
import tables, REG.M result aliases, callback changes to operands/current module,
normal binding failure and raised linker errors. Builtin calls cover all operand,
frame and Type alignments, changed FP and balanced callee references. Spawn uses
real emulator/kernel kprotect and construction boundaries; counters and queue
state must remain unchanged on rejection. Native error topologies are run in
separate processes, retaining stack/address instrumentation rather than disabling
checks after longjmp. Stable fixture images avoid stale automatic test storage
across these native unwinds.

The old code independently fails 32 parent probes: eleven LOAD/builtin families
in both WORD models, plus five SPAWN/MSPAWN families in both WORD models. These
include isolated original-result capture, original-MP capture, import alignment,
operand spans, MP header geometry, builtin FP/Type/map/reference handling and
spawn cells/targets/owners. Replay controls include `TRANSITION_SOURCE`,
`TRANSITION_WORD_BITS`, `TRANSITION_CASE`, `LOAD_STAGE`, `LOAD_MUTATION`,
`LOAD_TABLE_ALIGNED`, `LOAD_ERROR_FIELD`, `BUILTIN_MODE`, `BUILTIN_TYPE_ALIGN`,
`BUILTIN_BAD`, `TRANSITION_ERROR_ROUTE`, `STACK_XEC_SOURCE`, `STACK_WORD_BITS`,
`STACK_VARIANTS`, `SPAWN_PREFLIGHT_CASE`, `SPAWN_IMPORTED` and `SPAWN_ROUTE`.

## Priority architecture status

| Backend | Complete JIT + C entries | Guarded native fast paths | C-only |
| --- | ---: | ---: | ---: |
| AMD64 | 173/173 (100%) | 166/173 (95.95%) | 7 |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) | 7 |

The [per-command readiness table](README.jit-readiness.md) is unchanged. New
native execution was tested on AMD64; ARM64 compiler/guest checks do not replace
a physical ARM64 run. The compiler/runtime frame/reference ABI blockers in
[the native tool report](../../utils/tests/README.native-bootstrap.md) remain
before a full 64-bit VM boot. RV64 work is deferred. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are unchanged.
