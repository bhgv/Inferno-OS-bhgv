# Legacy native JIT entry validation

Run `python3 libinterp/tests/legacy-jit-inputs.py` from the repository root.
The test executes the actual leading declarations and validation statements
from MIPS, PowerPC and SPARC `compile` drivers, stopping at the first point
where compilation may acquire working storage. Encoders are not substituted
into this test because they are not reached. The production MIPS full-driver
publication/failure test remains `mips-publish.py`.

Each backend runs 21 cases with native and 32-bit VM words (126 cases total).
Invalid module and instance sentinels, missing program/entry/export/type
storage, compiled modules, mismatched/negative/overflowing sizes, negative
link counts and overflowing type allocation sizes must fail without changing
compiler globals or module data. Valid inputs, including zero descriptors,
are accepted by the preflight. Patch storage reserves a terminal entry; its
complete-driver regression is `legacy-jit-fallthrough.py`. This is shape validation, not a complete
verification of bytecode, relocation tables, frame indices or native code.

SPIM includes the same MIPS driver. RISC-V uses a different leaf-JIT contract
and has nil/sentinel/program-storage and size checks in `riscv-runtime.py`.
That host test exercises actual RISC-V compile/publication/cleanup functions;
its native callbacks are C stand-ins, not executed RISC-V instructions.

`LEGACY_JIT_SOURCE`, `CC` and `CFLAGS` allow baseline or sanitizer builds.
No module, frame, register or instruction layout changes are introduced.
