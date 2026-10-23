# Failure-safe 386 JIT publication

The 386 compiler now generates against a private module/data snapshot. Case,
long-case and computed-goto tables remain unchanged in the live module until
all code generation, cache synchronization and type helpers have succeeded.
Exports, instance links, exception PCs, entry PC and the compiled flag are then
published together. Repeated references to a Type share one rollback record;
existing helper code is preserved after a failed compile.

The entry stub and new type helpers are published after their own cache flush.
Private buffers remain reachable through an error-protected compile context,
so VM non-local errors release temporary allocations. A completed entry stub
may remain cached for retry. The patch table includes the end-PC slot required
by exception ranges and generated next-instruction jumps. Cache lengths use
bytes, and first/second-pass offsets are compared before publication.

ARM and 386 now reject H descriptors, negative descriptor sizes/map counts,
H metadata table pointers and missing module-data storage before snapshotting
or generating helpers. Nil optional descriptors retain their existing meaning.
This is not a complete verifier for every relocation table or malformed module.

Run from the repository root:

```sh
python3 libinterp/tests/386-publish.py
python3 libinterp/tests/arm-publish.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/386-publish.py
```

The 386 fixture uses the production compile driver, entry-stub/type-helper
lifecycle and table relocators, with byte-emission, allocator and OS boundaries
stubbed. It injects VM errors at 54 emission/logging/cache points, checks every
failure and retry, sweeps 25 allocation cutoffs, fails each of four cache flushes,
and checks phase mismatch, shared helpers, unchanged data/PCs, empty type tables,
invalid PCs/exception ranges and 14 invalid metadata configurations. Flush hooks
require the live module to remain unpublished and the flush range to fit its
allocation. The ARM fixture adds the same 14 metadata rejection configurations
and allocation-range checks to its existing 49-point error-injection suite.

`X86_PUBLISH_SOURCE` and `ARM_PUBLISH_SOURCE` select alternate backend files.
Pre-fix 386 sources compile in the fixture and fail at premature export
publication; pre-fix ARM sources fail on an H descriptor under UBSan.
The tests do not execute a whole native module, destructor ABI, scheduler or GC.
MIPS, Power and SPARC are outside this change. The 386 backend remains 32-bit;
these changes do not implement an x86-64 ABI.
