# Native64 series0084: mode-checked published LEA entries

The trusted predecode LEA8 step from0082/83 must not be bypassed by calling the
ordinary decoded native LEA entry. On WORD32/pointer64 that would truncate a
prepared result address. This series makes the published ARM64/x64/RV64 LEA
leaf entry alias its complete bridge. The bridge checks the active owner and
translation mode before reaching either the unchanged private native fast code
or the ordinary C fallback.

`cjitexec` refuses decoded LEA with `exCompile` if the Modlink/Module owner is
missing, H, misaligned/overflowing, in relocated native-PC mode, or has any
non-null cstep. It never invokes that translation callback. A decoded entry
cannot reconstruct a predecode LEA8 binding; normal xec and shared debugger/
coverage steps still execute the trusted callback before decode. Ordinary LEA
retains WORD width and its native implementation. The guard adds a C bridge
transition to ordinary LEA; this is not a performance improvement claim.

Both compiler cache validators reconstruct private fast-code addresses from
canonical layout, check exact bytes, require the LEA leaf/complete alias and
validate the tail against the private target. They do not trust a published
leaf pointer as its own bytecode proof. Cache mode changes remain quiescent;
nil/trusted/nil/trusted can reuse the same validated mapping. No Jit/Module/Heap
layout or wire IDs change. RV32 behavior is kept behind its existing RVXLEN
branches. MIPS/SPARC/PowerPC emitters are untouched.

This contract covers published entries and the decoded C executor, not arbitrary
jumps into an RX mapping's private interior or corruption of native host memory.
The private raw LEA code is intentionally ordinary WORD code. Native callers
must use the published ABI; this is not a native-code sandbox or file authority.

## Owning tests

The existing result-width fixture now also decodes a real prepared-frame LEA
and calls the decoded C entry, published leaf and complete entry. Across cache
absent/present, four mode phases, ordinary/extended stacks and three prepared
slots it checks768 probes per WORD model. Ordinary results preserve guard bytes;
translated modes, valid foreign callback, H callback, nil/H link/module and
native-PC mode refuse before writes, preserving the already-decoded PC and
leaving the protected error stack empty. The fixture restores owner fields and
REG between probes. It does not construct malformed low arbitrary addresses.

The full existing10,080-run result-call matrix and both10,080-run C instrumentation
matrices remain. New direct controls remove the cjitexec mode refusal (C/cache
absent and present), or publish the private raw LEA entry (cache present only).
Both must reach semantic status10 before a malformed result address can reach
CALL. The entry control deliberately skips repeated cache validation in that
isolated fixture: production cache validation would correctly reject its bad
table before execution. No compiler failure/signal/sanitizer failure counts as
control detection. Original width/priority/ordinary/mode controls remain.
Each model also rejects24 deliberately corrupted published LEA aliases with no
allocation, then restores the alias and reuses the same mapping. A third new
control removes only that cache alias check and must reach semantic status10
before any bad entry executes. New controls total four executions per target.
Instrumentation main loops still execute C; their separate direct-entry probes
also execute the published native tails/private fast code when a cache is present.

Generic compiler tests account for LEA's intentional leaf/complete alias; two
old corruption mutations that now produce no change are skipped. Other alias,
wrong-opcode, missing-entry, bytes and ownership corruption checks remain active.
Local native x64 bridge dispatch passes177,152 routes,6,181 compiler and295
integration checks per WORD model. Foreign ARM64 compiler preflight passes6,213
cases per model; this is emission/cache evidence, not ARM64 execution.

Local x64 optimized UBSan and focused fetch ASan/UBSan checks are being recorded;
the full three-target classic-mk CI evidence will follow the source commit.

## Remaining boundary and coverage

File CALL/LEA8 admission remains closed. Production constructor rejection of
incompatible callbacks, immutable container/declaration identity, result-call
caught recovery, general caller CFG/non-leaf graphs and whole-file out-of-line
C/RX positive/refusal/error goldens remain. No complete debugger GUI, Android
APK, new shared activation or parallel managed heap is claimed.

| Architecture | Available native fast paths | Complete JIT including C |
|---|---:|---:|
| ARM64 | 166/173 (95.95%) | 173/173 |
| x64 | 166/173 (95.95%) | 173/173 |
| RV64I | 87/173 (50.29%) | 173/173 |
| RV64 + Zmmul | 90/173 (52.02%) | 173/173 |
| RV64 + M | 103/173 (59.54%) | 173/173 |

Counts describe available paths, not time spent in machine code, every operand
form, full port readiness or the unchanged43/173 production file gate.
