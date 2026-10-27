# Priority AMD64/ARM64: complete dispatch and compiler/runtime ranges

This series stays on AMD64 and ARM64. Both have 173/173 complete JIT+C entries
and 166/173 guarded native fast commands. The seven complex C-only commands
remain ALT, NBALT, SPAWN, MSPAWN, LOAD, EXIT and RAISE. No RV backend changes
are part of this series. Full entry availability is not proof of a finished VM
port or of all operand domains; multi-instruction native blocks and physical
ARM64/complete Inferno boot validation remain outstanding.

## Complete-entry binding and ABI

The production compiler is now exercised with every byte opcode separately,
in both WORD models and both target backends. Tests require an entry exactly
for the selected active opcode, no entries for absent commands, in-mapping fast
and complete addresses, and byte-for-byte binding of each complete tail to its
actual fast function and cjitexec. Cached reuse must not allocate or replace the
mapping. Invalid/reserved-only modules must remain unpublished.

The expanded tail tests check code starts at zero and four modulo eight,
negative/short/exact capacities, full-width argument values, original return
contexts and unchanged stack/operand images. They now explicitly preserve SysV
rbx/rbp/r12-r15 and AAPCS64 x19-x29 plus d8-d15. AMD64/ARM64 FP control/status
checks remain active under Python -O. The existing direct fast route stays hot;
complex C handlers retain the safe tail return into the stable VM host caller.

## Shared SEND/RECV preflight

cchanleaf previously read decoded REG fields through a typed pointer, which
introduced a C alignment requirement absent from the native entry contract.
It also accepted output scratch overlapping live inputs, so a supposedly pure
preflight could corrupt REG, payload or channel metadata before native commit.

It now captures operand pointers with memmove and rejects nil/H/wrapping
scratch and overlap with REG, selected payload spans, Channel/Array Heap
headers, Type, peer-queue prefix or channel-owner cell before publication.
Success writes only its private transaction; refusal changes nothing. Native
AMD64/ARM64 still perform the bounded copy and counter/REG commit afterwards.
Blocking, queued, custom and mapped-pointer cases retain C. Deliberate payload/
owner-cell aliasing that follows C ordering remains supported.

Callers must still provide private writable scratch: numeric checks cannot
prove mapping, allocation ownership or general nonaliasing with arbitrary VM
storage. The native callers already reserve this transaction on their private
stack frames.

## Limbo optimizer

The optimizer previously narrowed host-long sizes and frame displacements to
int, evaluated unchecked byte-range sums, and advanced beyond the final Decl
when an aggregate extent lacked a tail. It could silently omit use/def rows or
dereference nil. Missing mapped-copy descriptors also reached typed reads.

Checked sizes, frame sums and declaration endpoints now use the signed positive
bound already used by frame layout. finddec resolves the complete range before
publishing its declaration count. Missing tails and invalid metadata use normal
compiler fatal diagnostics. Valid multi-declaration ranges, alignment gaps,
zero-byte ranges within a declaration, negative relative displacements yielding
a valid frame address, and the exact INT_MAX endpoint retain their behavior.

## Evidence

| Main matrix | Primary cases | Execution |
| --- | ---: | --- |
| Complete dispatch C/RX, AMD64 WORD32/64 | 364,120 | Real executor, RX code, compiler and xec |
| Compiler binding/preflight, ARM64 WORD32/64 | 9,292 | Production compiler; ARM code not called on AMD64 |
| Tail ABI/capacity, AMD64/ARM64 | 33,762 | Unicorn; every active opcode |
| Private channel preflight, WORD32/64 | 4,496 | Shifted REG/output and scratch-alias sweeps |
| Limbo optimizer ranges, WORD32/64 | 190,448 | Production functions with real compiler headers |
| Total | 602,118 | Primary cases, not individual predicates |
| Complete-entry capacity probes | 264,192 | All byte opcodes, both WORD models, nil/non-nil fast targets |

Additional regressions cover buffered C/RX transfers (1,592,256 accepted and
832 declined cases), 328,960 AMD64/ARM64 buffered guest cases, publication and
failure cleanup in both WORD models, channel queues and Limbo bit sets, frame
layout, case cells, descriptor maps and folding. The parent commit 637d618 fails
eight selected probes: six optimizer errors and the two channel preflight
defects. The complete production optim.c also compiles as a host object with
implicit function declarations treated as errors.

UBSan/NDEBUG covers the main synchronous C paths and explicit C throws.
ASan/UBSan covers optimizer fatal diagnostics in separate children, ARM64
compiler preflight, nonthrowing AMD64 dispatch and private channel preflight.
The stack-only optimizer fixture disables LeakSanitizer's /proc-dependent scan;
address/undefined checks and stack instrumentation remain enabled. Existing
C-longjmp ASan limitations still require BRIDGE_NO_ERRORS=1 for dispatch.
This is not an Inferno/compiler boot or physical ARM64 execution test. Tk,
security/crypto and MIPS/SPARC/PowerPC JIT sources are unchanged.

Reproduce the main matrices:

```sh
python3 libinterp/tests/jit-dispatch-native.py
BRIDGE_BACKEND=arm64 BRIDGE_PREFLIGHT_ONLY=1 python3 libinterp/tests/jit-dispatch-native.py
python3 -O libinterp/tests/jit-dispatch-jit.py
python3 libinterp/tests/jit-dispatch-coverage.py
python3 libinterp/tests/buffered-transfer-native.py
python3 limbo/tests/optimizer-ranges.py
python3 libinterp/tests/jit-readiness.py --check
```
