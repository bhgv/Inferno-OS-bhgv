# Integer branch and jump operand preflight

The independent checkpoint was developed against experiment commit
`14c3d2c2736936eade4eadfa25e87c8ce5e404ec`. Concurrent upstream commit
`2664a10f60e6eb06b2e975a4d02cff6fb0dc440e` supplies the production fixes for all
eighteen BYTE/WORD/LONG BEQ/BNE/BLT/BLE/BGT/BGE handlers and JMP in the C
interpreter and AMD64/ARM64 emitters. Those incoming production implementations
and their branch-slot tests are preserved; this commit adds independent C/RX and
instruction-emulation oracles and registers them alongside the upstream tests.

Both scalar source slots must be non-null, non-H and have a non-wrapping
complete native address span before either value is read. Malformed used
slots raise exBounds in C. A declined native leaf returns zero without stores,
leaving the same C handler authoritative. BYTE comparisons remain unsigned;
WORD and LONG comparisons remain signed in their own widths.

Only taken predicates inspect their native-pointer-width target slot. False
predicates neither load REG.d nor validate/read its target. A taken branch or
JMP validates that slot before loading it and publishing PC. Target values keep
the existing decoded-PC contract; these checks bound slots, not code destinations
or arbitrary allocation ownership. JMP never inspects its unused scalar operands.
All captures remain byte-safe, including overlapping operands/targets.

ARM64 now relocates the inverse conditional branch over the complete guarded
PC publication path. Keeping the previous fixed four-instruction displacement
after inserting guards would make false comparisons enter the taken path.
AMD64 keeps its displacement patching over the actual emitted taken path.
The shared integer ALU guards retain their previous validation order.

## Independent validation

```
python3 -O libinterp/tests/integer-branch-native.py
CFLAGS='-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all' python3 -O libinterp/tests/integer-branch-native.py
python3 -O libinterp/tests/integer-branch-jit.py
python3 libinterp/tests/integer-branches.py
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/slice-tokenize-suite.py --core --jit
```

The new tests use explicit if/error predicates, including under NDEBUG/Python -O.
They are registered in the existing checkpoint driver. The independently developed
implementation passed the complete 197-driver selection plus seventeen additional
compiler drivers before synchronization. Final incoming production code is checked
again with both new and upstream branch checkpoints and adjacent arithmetic,
shift, ALU and coverage drivers; the 197-driver pass is not presented as a full
rerun of the combined 199-driver selection. Across all 175 opcode IDs and both
WORD widths, 700 same-layout sizing/image probes produce byte-identical AMD64/ARM64
emitter outputs for the incoming and independently tested implementations. This
is an integration check, not a proof for every possible private ABI layout.

* C and actual RX AMD64 execution: 204,240 cases in WORD32 and 205,008 in
  WORD64. Each case checks a separate numeric predicate and complete memory/REG
  image; rejected leaves also execute the real C fallback. Matrices include
  signed extrema, high WORD bits, every byte alignment, exact/partial target
  aliases, nil/H/wrapping spans, poisoned sibling sources, true-target errors,
  false-target nonaccess and unused JMP inputs. Both models pass ASan/UBSan.
* AMD64 and ARM64 instruction emulation: 130,672 cases each, across both WORD
  models. Independent Python integer predicates and complete byte images check
  native completion/decline, unaligned/aliased targets and PC. Read/write hooks
  reject false-target inspection and any declined store. FP state is unchanged.
  Production sizing, every insufficient output capacity, exact-capacity images
  and a trailing canary are checked without increasing private emitter capacity.
* Parent-version substitution is available through INTEGER_BRANCH_SOURCE,
  INTEGER_BRANCH_EMITTER_DIR, INTEGER_BRANCH_OP and INTEGER_BRANCH_ARCHES.
  All nineteen C and all thirty-eight AMD64/ARM64 command/backend probes
  reproduce the parent memory-access failures with the same test drivers.
* The existing production-REG integer comparison fixture remains active; its
  valid-only error stub now supplies the newly referenced bounds identity.

The sanitizer runtime warns that executable-name lookup and a longjmp stack
notification are unavailable in this sandbox. No sanitizer failure was reported
in these new tests. Address/undefined/stack instrumentation remains enabled;
only unsupported LeakSanitizer process enumeration is disabled for the stack-only
fixture. Full Inferno boots and physical ARM64 execution remain separate checks.

## Dis coverage

Production emitters still report 175 slots: INOP is invalid and IECLR unused,
leaving 173 active commands. These are corrected existing paths, not new opcode
implementations. WORD32 and WORD64 have the same counts.

| Backend | Active | Has native path | Interpreter only |
| --- | ---: | ---: | ---: |
| AMD64 | 173 | 164 | 9 |
| ARM64 | 173 | 164 | 9 |
| RV64 + M | 173 | 103 | 70 |
| RV64 + Zmmul | 173 | 90 | 83 |
| RV64I | 173 | 87 | 86 |

The nine AMD64/ARM64 interpreter-only commands are IALT, INBALT, ISPAWN,
ILOAD, IMSPAWN, IEXIT, ISEND, IRECV and IRAISE. A counted native path may accept
only guarded operand subsets and fall back to C for other inputs; 164/173 is
availability, not complete native semantic coverage or block-JIT completion.
No MIPS/SPARC/PowerPC JIT or Tk/security/crypto production source is changed.
