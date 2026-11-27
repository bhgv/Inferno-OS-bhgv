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

Local x64 optimized nonrecovering UBSan result-call matrices and all13 strict
result controls pass. Focused fetch ASan/UBSan checks pass242,562 parent predicates
per WORD model; ASan emits a `__asan_handle_no_return` warning around fixture
nonlocal unwinding. This is a focused run, not whole-runtime sanitizer
certification. Owner, frame, direct-target, scalar-channel and module-transition
closures pass after the error-identity updates. Twelve RV64 compiler cache
configurations pass on the host; those are cache/emission checks, not RV execution.

[CI41525531140](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41525531140)
passes all three targets at source `4a51bda2919d39e567ede7cfe605ce7fb77f5ae5`,
including fixture compatibility updates: native ARM64, native x64 with optimized
nonrecovering UBSan owning C gates, and RV64/QEMU. Compiler/runtime implementation
is `ca506976d17dc91b1a5b899c4d60b8a21e4d6971`; subsequent source commits add
cache probes and complete the isolated test environments.

All targets pass768 direct-entry and24 cache-poison probes per WORD model in
each of the ordinary/result and two instrumented fixtures. All13 strict result
control executions pass. The10,080 accepted/96 refused raw-call matrix and its
two C-instrumentation variants remain green. Earlier metadata/CFG/initialization/
typed-return/caller proofs and heap/stack/caught-handler gates remain green;
they do not establish new result-call caught recovery or parallel GC correctness.

The extracted executor also gains an `exCompile` dependency. Legacy isolated
fixtures now supply that existing runtime error identity. The freestanding RV64
bridge fixture supplies an ordinary synthetic Module/Modlink owner, byte-copy
support and the missing native sentinel/layout definitions. It still tests a
synthetic register layout, not the full runtime structure ABI. Its existing
`fence.i` is encoded with unchanged bytes so assemblers requiring explicit
Zifencei naming can compile the RV64I arithmetic profile. The RV64 port driver
now runs this existing >4GiB complete-tail gate across I/M/GC and WORD32/64.
Full owning result/heap tests remain separate. Host syntax checks are not counted
as target execution. The RV64 CI runner passes six I/M/GC x WORD32/64 executions,
each with173 opcode tails/1,038 C/fast outcomes and >4GiB pointers, plus its two
existing bridge mutation controls. These legacy ABI controls are separate from
the13 strict semantic result-call controls.

## FlashVM coordination

FLASHVM-0074 at `75edaf0feddd837ecc649042c04175b2a4a60722` independently replays
and agrees with0083's narrow C instrumentation integration. Its0084 receipt is
not a replay or agreement on the new published-entry/cache paths. Our response
acknowledges that distinction. Runtime0.60.0's typed native RC proposal remains
an independently documented foundation; no Heap/Type/JIT atomic conversion or
parallel managed heap is imported into Inferno.

## Downloads and archive verification

Downloaded all three artifacts and verified their ZIP digests against GitHub,
ELF64 machines183/62/243 and new positive/control logs. ARM has184 logs, x64183,
RV185 including the additional complete-tail gate. Six packaged x64 loop/case/
procedures JIT0/1 processes retire with the expected modules, empty exception
and empty scheduler. The common `procedures.dis64` SHA256 stays
`6202fd7c8191798cbd22bf15dfe8fecda06529b2cb21c25abb70545b677ff3c7`.

These archives contain experimental Linux scalar consoles and complete gate
logs. They are not APKs or a full managed GUI/shell. Artifact retention is14 days.

| Target | Result | Archive |
|---|---|---|
| ARM64 native | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41525531140/artifacts/13180497083) |
| x64 native, UBSan C gates | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41525531140/artifacts/13181312491) |
| RV64/QEMU | PASS | [download](https://github.com/bhgv/Inferno-OS-bhgv/actions/runs/41525531140/artifacts/13181175201) |

| Target | ZIP SHA256 |
|---|---|
| ARM64 | `7da046276b05e84dc7f7c56262aae762cd18e62c77122302e83cde38ea4bdf4b` |
| x64 | `e0e479e92bf92934b62842e592277b788400e7a6fcb9fc5bd5ba907c98f8683b` |
| RV64 | `ab948b0b818c4e39b14491dba619181c5b1dd0da84c5c4959538f5891d929f96` |

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
form, full port readiness or the unchanged43/173 production file gate. Counts
reflect the Linux native layouts used by these gates, not certification of every
Android/Windows/macOS ABI.
