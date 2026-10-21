# ARM JIT publication and rollback

The ARM JIT now checks cache synchronization for the entry trampoline, Type
initialize/destroy helpers and the generated module. The trampoline and Type
entry pointers are published only after their code has been synchronized.

Module generation stages case/goto table changes in a copied data segment and
a shadow Module. Until publication succeeds, the original data, Dis program,
entry point, exported/dynamic links and exception handlers remain unchanged.
The main code is synchronized before generating Type helpers. Old Type entries
are recorded before generation; on a controlled failure, newly allocated
helpers are freed and existing/shared entries are retained. Allocation failure,
cacheflush failure and pass-offset mismatch return 0 with the interpreter
representation intact, allowing a retry. All rollback storage is allocated
before mutation. Zero-type modules no longer depend on malloc(0) succeeding.

Publication validates module/link PCs and exception ranges before indexing the
patch array. That array now includes patch[nprog], the end of the instruction
stream before native macros: an exclusive exception-range end equal to nprog
no longer reads outside the array. This fix currently applies to ARM only.

## Regression

```sh
python3 libinterp/tests/arm-publish.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 libinterp/tests/arm-publish.py
```

The test extracts production compile, preamble, typecom, case/casel/goto
relocators and exception patching. Host scalar definitions model ARM32;
integer instruction emission and OS boundaries are stubs. Fault injection
covers every allocation cutoff in the fixture, each cacheflush stage, successful
retry, duplicate tables, aliased/shared Type entries, dynamic links, pass-offset
mismatch, malformed PCs and end-of-stream exception ranges. Assertions inspect
original tables and link PCs during cache synchronization, before publication.
Explicit allocation accounting catches leaks/double frees on these return paths;
LSan itself is unavailable in this sandbox. ASan stack checking is enabled.

This supplements arm-vfp.py, which runs generated VFP instructions; it does not
execute a complete generated module, GC, OS cacheflush or the actual scheduler.
Host syntax checking of the complete comp-arm.c also passes.

## Type helper capacity and instruction encoding

Type helper measurement now allocates a checked capacity proportional to the
pointer map instead of a fixed 4096 words. Each byte describes at most eight
pointers; 128 words per byte plus a prologue block conservatively cover memory
address construction, calls, literals and pool branches. The bound is checked
before multiplication and before map-index shifts. The combined helper byte
size and final emission count are also checked before publication.

ARM condition fields now shift an unsigned word when placing bits 31:28.
The former signed-int C<<28 was undefined for common conditions including AL.
The emitted bit patterns are unchanged.

```sh
PYTHONPATH=/path/to/arm-test-deps python3 libinterp/tests/arm-types.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  PYTHONPATH=/path/to/arm-test-deps python3 libinterp/tests/arm-types.py
```

This harness uses production integer emitters, literal-pool handling,
comi/comd and typecom. A host executable checks large/dense/sparse maps, all 16
condition encodings and allocation/cache failure cleanup under ASan/UBSan.
For execution, a host shared library retains UBSan but omits ASan to avoid
injecting its runtime into Python. Unicorn executes initializers for maps up
to 4096 bytes (32768 pointer slots), verifies every marked/unmarked slot and
checks surrounding canaries. There are 84 initializer/failure scenarios plus
the encoding assertions and descriptor-bound checks. The target-specific
sizeof(WORD*) expression is evaluated as 4 in this ARM32 model. Destructor
instructions are emitted and size-checked, but native destructor calls and
GC integration are not executed.

## Still open

- Non-local VM error() unwinds from the instruction emitter do not currently
  release all compiler scratch allocations. Staged table generation protects
  the module data, but full exception cleanup needs a runtime protection boundary.
- The legacy backends share similar table/publication patterns; these ARM tests
  do not establish their safety. RISC-V's leaf sidecar already avoids relocation
  and publishes only after successful synchronization.
- Native-device cache behavior, executable allocation and migration across CPUs
  remain target integration checks.
