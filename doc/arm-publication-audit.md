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

## Still open

- Non-local VM error() unwinds from the instruction emitter do not currently
  release all compiler scratch allocations. Staged table generation protects
  the module data, but full exception cleanup needs a runtime protection boundary.
- Type helper sizing still uses a fixed 4096-word scratch buffer. Large pointer
  maps need bounded/dynamic generation and separate regression coverage.
- The legacy backends share similar table/publication patterns; these ARM tests
  do not establish their safety. RISC-V's leaf sidecar already avoids relocation
  and publishes only after successful synchronization.
- Native-device cache behavior, executable allocation and migration across CPUs
  remain target integration checks.
