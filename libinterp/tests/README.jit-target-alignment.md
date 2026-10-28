# ARM64 complete JIT target alignment and compiler query checkpoint

The ARM64 complete-entry emitter now rejects executor and non-null fast-entry
addresses with any of the low two bits set, before sizing or writing caller
output. AArch64 instruction targets must be four-byte aligned: the previous
emitter encoded an entry which would fault on branch, or pass an invalid fast
function to the C dispatcher. This is an emission preflight guarantee, not a
claim that any aligned numeric address belongs to executable mapped storage.

AMD64 intentionally keeps byte-addressable targets. C-only entries still accept
a null fast target. Both backends retain tail transfers into the C executor,
so a C handler may free the source module mapping without leaving a return
address inside that mapping.

`jit-dispatch-coverage.py` checks all three bad alignment patterns for both target
arguments, for every active opcode in WORD32 and WORD64. ARM64 rejection checks
both sizing and unchanged output; AMD64 acceptance is checked independently.
The expanded `jit-dispatch-jit.py` exercises accepted tails in Unicorn and checks
rejected output, arguments, capacity, original stack/return context and FP state.
The unchanged parent ARM64 emitter fails the new target test (exit 7) after the
AMD64 comparison passes; no generated invalid code needs to execute to expose it.

Verified commands:

```sh
python3 libinterp/tests/jit-dispatch-coverage.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-dispatch-jit.py
python3 libinterp/tests/jit-readiness.py --check
python3 libinterp/tests/jit-coverage.py
python3 limbo/tests/node-queries.py
python3 utils/tests/native-bootstrap.py
```

Unicorn reports 29,397 AMD64 and 13,481 ARM64 cases. Native AMD64 C/RX tests pass
both WORD models, each with 177,152 routes, 6,183 compiler cases and 295
integration checks. ARM64 compiler checks run on the host without executing
foreign code. The accompanying [compiler series](../../limbo/tests/README.node-queries.md)
removes depth-proportional C stacks from five AST queries and adds checked node
counting and paired-tree temporary ownership.

## Verified opcode availability, 2026-10-28

| Architecture | Complete JIT entry, native + C | Native fast path | C through JIT only |
| --- | ---: | ---: | ---: |
| x86-64 | 173/173 (100%) | 166/173 (95.95%) | 7/173 (4.05%) |
| ARM64 | 173/173 (100%) | 166/173 (95.95%) | 7/173 (4.05%) |

The seven C-only commands are ALT, NBALT, SPAWN, MSPAWN, LOAD, EXIT and RAISE.
No opcode needs a native body merely to count as supported by a complete entry.
See the verified [173-row per-command table](README.jit-readiness.md) and
[machine-readable CSV](jit-readiness.csv). Their values remain unchanged.
Percentages describe entry availability across the two WORD fixture models,
not the fraction of operand states accelerated or overall port completion.

RV64 fast availability remains 103/173 (59.54%) with M, 90/173 (52.02%) with
Zmmul and 87/173 (50.29%) for RV64I. These counts do not establish equivalent
complete native+C entry coverage for RV64.

Physical ARM64 execution, full Inferno boot and coordinated 64-bit compiler,
loader, frame and GC ABI acceptance remain outstanding; see
[the existing ABI blockers](../../utils/tests/README.native-bootstrap.md).
This series changes no Tk, security/crypto or MIPS/SPARC/PowerPC JIT sources.
