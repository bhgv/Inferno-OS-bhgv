# Dis to C

`dis2c` translates a Dis module produced by either Limbo compiler into C99.
The generated module links into the matching Inferno runtime. It specializes
operand decoding for every instruction, emits scalar moves and modular integer
arithmetic directly, and calls the existing VM helpers for other operations.
GC, channels, exceptions, imports and system calls retain their runtime semantics.
This is an initial runtime-linked AOT backend, not a standalone libc executable.

## Build and translate

From the repository root, with a host C99 compiler and GNU make:

```sh
make -C utils/dis2c
utils/dis2c/dis2c --check program.dis
utils/dis2c/dis2c -n program_aot -o program_aot.c program.dis
```

`-o` defaults to standard output. `-n` defaults to `dis_module`; use distinct
prefixes when linking multiple translations. Python is only needed for tests or
`make -C utils/dis2c regen` after changing the opcode tables. `CC`, `CPPFLAGS`
and `CFLAGS` can be supplied to make.

The input must be a complete Dis file smaller than 8 MiB, as for `readmod`.
Wire structure is checked before opening the output. The runtime loader remains
authoritative for native type layouts, data initialization and signature checks.

## Link into Inferno

1. Put the generated file in `libinterp`, for example `libinterp/program_aot.c`.
2. Add `program_aot.$O` to `OFILES` in `libinterp/mkfile`.
3. Declare and call `int program_aot_register(void)` from the selected platform's
   module initialization, before any load of this Dis module. Check its return
   value. Registration must happen before interpreter startup or under the VM lock.
4. Rebuild `libinterp` and the emulator together. Keep the original Dis file in
   the normal module search path; ordinary Limbo `load` now selects the C code.

Registration uses static storage. Keep the generated object linked for the entire
runtime lifetime; unloading its library after registration is unsupported.
There is no automatic compiler constructor or change to the Limbo `load` syntax.

For a C embedding, the generated entry points are:

```c
int program_aot_register(void);
Module *program_aot_load(char *path, Dir *dir);
```

The second function registers and loads the embedded image using `parsemod`.
Both arguments must be non-nil; supply the module identity and `Dir` appropriate
to the embedding. Normal loader ownership and VM locking rules apply.

## Example

With a working Limbo compiler in the host path, from the repository root:

```sh
limbo -I module -I utils/dis2c/examples -o sum.dis utils/dis2c/examples/sum.b
utils/dis2c/dis2c -n sum_aot -o libinterp/aot_sum.c sum.dis
limbo -I module -I utils/dis2c/examples -o driver.dis utils/dis2c/examples/driver.b
```

Add `aot_sum.$O` to `libinterp/mkfile`, register `sum_aot_register()` as described
above, and rebuild. In that emulator, run `driver.dis` from an Inferno directory
containing both Dis files. Its interpreted caller loads the C-backed Sum module
and prints `sum(10) = 55`. The compiler accepts the same input/output options in
both implementations; the translator itself does not depend on either frontend.

## Checked native decoding and scalar operations

Generated code now shares the checked inline address helpers in `dec-addr.h`,
uses alignment-independent, overlap-safe scalar access, and validates complete
native program/header spans. Sixteen simple operations remain inline; complex
operations retain their VM C handlers. Regenerate older emitted files with the
matching runtime. See [native-slot regression evidence](tests/README.native-slots.md).

## Runtime contract and limits

The loader attaches a translation only when the **entire original image** and
instruction count match. Changed data, signatures or metadata require regeneration.
Unregistered images follow the existing interpreter/JIT route. `dontcompile` and
`DONTCOMPILE` disable attachment; a registered translation can satisfy
`MUSTCOMPILE`. Signed modules still pass through the normal signature policy.

The original `Inst` array, Dis PCs, data initializers and exception tables remain
available. C and interpreted modules can call each other through existing VM
helpers. Each translated step consumes the same scheduler quantum as one Dis
instruction. A declined C step falls back to the existing execution path.
`Module.compiled` remains the native-PC/JIT flag; C translation uses `Module.cstep`.

The Dis wire format is unchanged. The native `Module` structure is extended, so
rebuild the runtime and generated objects together. Generated C uses the current
Inferno ABI and is suitable for compiling with future host toolchains once the
runtime itself supports that target ABI. This does not supply x86-64, arm64 or
riscv64 JITs, nor remove the existing VM's pointer/layout constraints.

The initial backend executes a switch per instruction and still calls VM helpers
for complex operations. No speedup is promised. Whole-function optimization,
standalone executables and dynamic unload support are future work.

## Verification

```sh
make -C utils/dis2c test
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  ASAN_OPTIONS=detect_leaks=0 python3 utils/dis2c/tests/test.py
```

Tests compile generated C against real headers and compare the C and interpreter
paths using production loader, decoder and execution-loop code. They check a loop,
all opcode dispatches, 144 addressing combinations, signed operand boundaries,
scheduler quanta, interpreter/JIT fallback, stale images, imports, exception
metadata, `.mp` exports, signature rejection and malformed/truncated files.
Complex helper bodies are dispatch stubs in the differential fixture; this is not
a full emulator boot or execution test of every opcode's side effects.

Set `LIMBO` to a working host compiler command to additionally compile the example
from Limbo source and compile its generated C. Without it, the suite uses encoded
Dis fixtures and does not claim frontend or full-system execution coverage.
