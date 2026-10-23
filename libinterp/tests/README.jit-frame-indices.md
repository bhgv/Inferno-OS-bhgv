# Native-PC JIT frame selection and allocation failures

Run:

```sh
python3 libinterp/tests/jit-frame-indices.py
python3 libinterp/tests/mips-publish.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/jit-frame-indices.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/mips-publish.py
```

The frame driver extracts the complete production IFRAME switch arms for ARM,
386, MIPS, SPARC and Power. It also executes each backend's actual tinit
allocation statement with a poisoned allocator. There are 108 cases per backend
(540 total): immediate and register/memory sources, all table positions, negative
and out-of-range indices, nil tables/descriptors, negative and undersized frames,
and minimum/full-sized frames. Only valid constant descriptors may mark a Type
for helper generation or reach the native emission path. Invalid constants
must emit an interpreter fallback, where the checked frame handler reports the
runtime error. Selection does not eagerly reject an unreachable bad instruction.
The host test uses native pointer/WORD sizes and each backend's actual uchar or
ulong mark element type. It is not an emulated 32-bit target execution test.

The MIPS publication driver executes the entire production compile function.
Instruction/type encoders, executable cache synchronization and exception-table
relocation are boundary stubs. Both passes and their code-size offsets execute;
entry/import/export relocation and publication execute in the real driver.
Poisoned tracked allocation covers failures of patch, tinit, preamble and final
code buffers, both with and without a cached preamble. Each failed attempt is
retried. Twenty repetitions give 240 cases, with exact allocation accounting,
unchanged module metadata on these failures, and successful final publication.
Only the frame-selected Type may request a helper. A successfully cached preamble
is retained after a later allocation failure and released by the fixture at end.

Both drivers pass strict UBSan and ASan, with stack instrumentation enabled.
Only unsupported LeakSanitizer process inspection is disabled.

Independent counterexamples are available through JIT_FRAME_SOURCE_DIR,
JIT_FRAME_ARCH and JIT_FRAME_CASES=indices|initialization. The previous switch
arms produce ASan buffer overflows independently in all five backends; the
previous allocations leave poisoned helper marks in all four non-ARM backends.
MIPS_PUBLISH_SOURCE substitutes an earlier compiler without changing the fixture.
Its old uninitialized-mark failure occurs first; after changing only that
allocation to clear the marks, UBSan independently reports a null patch-table
store on allocation failure.

These tests do not execute native machine code, establish full compiler rollback
on VM errors in typecom, validate all malformed instruction metadata, fix every
native stack arithmetic path, or provide x64/arm64/RV64 JIT ports. Those remain
separate audit and implementation work.
