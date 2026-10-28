# Complete decoded JIT dispatch: native fast path plus C

Latest [list/channel runtime callback checks](README.jit-callback-alignment.md) cover the six helper identities used by guarded fast paths.

Previous [ARM64 target-alignment checks and compiler-query checkpoint](README.jit-target-alignment.md) retain full native+C dispatch availability.

AMD64 and ARM64 now publish a complete Jit.dispatch entry for every active
opcode used by a compiled module, including modules containing only complex C
operations. There are 173 supported active opcode IDs in both WORD models:
166 have guarded native fast implementations, and seven use C through the JIT
entry: ALT, NBALT, SPAWN, MSPAWN, LOAD, EXIT and RAISE.

The previous fast-path metric remains 166/173 (95.95%). Complete decoded entry
availability is 173/173 (100%). Neither number measures all operand-state
combinations, overall VM port completion or performance. The generated
[per-command table](README.jit-readiness.md) and [CSV](jit-readiness.csv) expose
both percentages, execution mode and native-domain restrictions.

## Boundary and lifetime contract

Each native complete entry tail-jumps to cjitexec(REG*, opcode, fast). SysV uses
rdi/esi/rdx; AAPCS64 uses x0/w1/x2. The stub does not allocate a frame or replace
the original return context. Its original caller must be stable host code, as
in the current decoded VM loop. A future module-owned compiled block must use
an appropriate lifetime/exit barrier before crossing this boundary; simply
CALLing a complete entry from an unpinned native block would not satisfy it.

The shared executor requires the actual global R, which is already decoded and
has its original Dis PC advanced once. It tries the captured fast function once;
on zero it looks up and invokes the current optab handler. Missing handlers
raise exOp. Reserved/unknown opcode IDs, foreign REG identities and an H fast
pointer are rejected without invoking callbacks. No per-Prog REG copy is hidden
at this boundary and no PC advance or scheduler counter reset is added.

A handler may block, set IC/t, switch R.M/MP/PC, enter the existing exception
machinery, or destroy its source Module and Jit mapping. The executor retains no
Module/Jit pointer and accesses neither mapping after callbacks. A JMP/BR stub
can therefore return directly into the original host caller. An ordinary
module-owned CALL/BLR wrapper would leave an unsafe return address if the C
handler releases the mapping.

The decoded xec loop keeps its existing direct Jit.entry call for hot native
successes and normal C fallback after fast refusal. It uses complete entries
for commands without a fast entry. Complete Jit.dispatch entries are also
independently callable for all supported opcodes, including fast-plus-C cases.
This preserves the direct hot route and does not retry a refused fast path.

This remains a per-opcode JIT using original Dis PCs, frames and GC metadata.
It does not implement multi-instruction native blocks or promise that an extra
C boundary speeds up a complex operation. Legacy RV64 fast tables continue to
work; their newly appended dispatch fields are zero-initialized by the existing
mallocz allocation. MIPS/SPARC/PowerPC JIT sources are unchanged.

## Publication and compiler fixes

The compiler sizes guarded fast bodies and fixed-size complete stubs, emits all
fast targets first, then binds complete entries to those actual addresses.
Both tables share the same mapping, cache synchronization and RW-to-RX
publication. The Module is updated only after every emission and protection
step succeeds. Complete entries are 32 bytes on AMD64 and 44 on ARM64; each
used opcode gets one stub. Jit has an additional 256-pointer dispatch table.
The original fast table, its offsets and decline convention remain available.

Compiler preflight now bounds and aligns the public Module header before any
typed read, checks the complete bytecode span and validates cached Jit headers,
nprog, code identities, positive size and nonwrapping code spans. Invalid inputs
return failure before allocation/publication. Opcode scanning uses byte reads
at offsetof(Inst, op), avoiding a typed unaligned Inst access. This does not
claim execution support for arbitrary unaligned bytecode outside that scan.
Numeric span checks cannot establish mapping or allocation ownership.

INOP, IECLR and unknown byte values receive no complete entry; the normal VM
bad-op/spare handling is retained. A reserved-only module still cannot publish
code. A C-only active module now successfully publishes its complete entries.

## Evidence

The [priority AMD64/ARM64 continuation](README.priority-dispatch-ranges.md) adds
all-byte compiler binding, explicit callee-saved ABI checks, pure channel scratch
validation and Limbo optimizer range fixes.

| New driver | Primary checks | Scope |
| --- | ---: | --- |
| jit-dispatch-native.py | 364,120 | Real C executor, AMD64 RX tails, compiler and xec, WORD32/64 |
| jit-dispatch-jit.py | 33,762 | AMD64/ARM64 Unicorn argument/tail/stack/FP/capacity cases |
| ARM64 compiler-only WORD32/64 | 9,292 | Production binding/preflight, no foreign execution |
| Primary total | 407,174 | Full UBSan/NDEBUG matrix |
| jit-dispatch-coverage.py | 264,192 capacity probes | All byte opcodes, both targets/WORD models, nil/non-nil fast targets |

Every active opcode has a distinct test C handler, so a wrong opcode argument
or optab index cannot silently pass. RX routes cover nil/declined/completed
fast functions, nonzero negative completion, unaligned/overlapping operand
cells, complete REG/data images, PC/IC/t/module updates and unchanged FP state.
Foreign REG objects are rejected without reads or mutations. Integration tests
exercise real ADDW, a real DIVW zero exception after native refusal, a missing
handler, and an ordinary C exception crossing the tail entry.

The real xec test frees the source Module and its RX mapping in a C-only LOAD
handler, continues decoding in a second compiled module, and stops at the
callback-selected scheduling boundary. It verifies decoder/cache entry counts
and final Prog.R state. Another test poisons the complete ADDW wrapper and
confirms that normal VM execution still takes the direct successful fast path.

Compiler cases cover wrapping Module/program/cache/code spans, stale nprog,
missing/nonpositive code storage, and shifted program opcode scanning. Four
parent 87785ec fixture selections reproduce the original header/program/cache
or unaligned-scan failures. BRIDGE_COMPILER_SOURCE selects the unchanged parent
compiler text with the current fixture declarations.

The complete C/RX matrix passes UBSan with NDEBUG. ASan/UBSan also passes the
non-throwing routes, compiler preflight and mapping-destruction integration with
BRIDGE_NO_ERRORS=1; production stack instrumentation stays enabled. The three
explicit C-longjmp cases per WORD model remain part of the full UBSan run,
consistent with the documented sandbox ASan stack-shadow limitation.

Forty-seven selected driver configurations pass, including AMD64/ARM64/RV64
publication, both WORD models, RV64I/Zmmul, legacy runtime dispatch, the prior
buffered channel, mapped/raw copy, ownership, arrays, and Limbo layout/map
regressions. Readiness generation independently verifies both fast and complete
entry availability without creating or rewriting fixture bytecode caches. Three
incoming test fixes were preserved before publication: channel span/peer helper
extraction, the exact LeafLayout callback position, and readonly readiness imports.
The full existing leaf64-jit regression passes 1,150,144 checks across AMD64,
ARM64 and RV64; its additional guard selection passes 1,171 checks. This is not the full suite or an Inferno/compiler boot.
Physical ARM64/RV64 execution remains unverified. Tk and security/crypto are
outside this change.
