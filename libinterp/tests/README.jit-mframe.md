# Checked imported frames in the 386 and ARM JITs

`mframe` uses a signed Limbo link index. Nonnegative indices select imports;
negative indices name exports and cannot be used as offsets into `links`.
The 386 and ARM selectors now send negative immediates and register operands
to the checked VM path. Positive immediates retain native allocation after
checking the module pointer, import count, descriptor pointer, minimum frame
size and initialization helper. Missing helpers also use the VM path.

The fallback checks reject nil modules, H descriptors, invalid export owners
and negative import counts. Frame sizing uses a signed minimum comparison so
a negative size cannot reach stack growth after replacing the source register.
Constant `frame` selection also rejects H descriptors in these two JITs.

Run from the repository root:

```sh
python3 libinterp/tests/jit-mframe.py
STACK_CASES=frameerrors python3 libinterp/tests/stack-startup.py
JIT_FRAME_ARCH=386 python3 libinterp/tests/jit-frame-indices.py
JIT_FRAME_ARCH=arm python3 libinterp/tests/jit-frame-indices.py
```

The new driver requires a host C compiler and Python `unicorn`. It extracts the
production `commframe` body and `IMFRAME` switch arm, retaining instruction
encoding macros and x86 ModRM encoding. Each CPU model executes 2700 metadata
combinations and checks another 18 register-operand selections (5436 total).
Coverage includes signed index extrema, nil/H modules and types, negative and
exhausted counts, undersized frames and absent initialization helpers. Memory
read hooks check that rejected import indices do not read the table slot or
the descriptor. Constant, call, fallback and selected load emission are stubs;
execution stops at native-helper/fallback entry. A full native ABI, external
calls, helper execution and physical CPUs are not covered.

Set `JIT_MFRAME_SOURCE_DIR` to an alternate backend directory and optionally
`JIT_MFRAME_ARCH=386` or `arm`. Both pre-fix backends compile in this fixture
and fail during emulation on an invalid metadata read.

`stack-startup.py` adds 15 VM rejection cases for each emu/os and native/WORD32
combination, checking unchanged registers and destination with no allocation.
It also covers nil/H export owners and negative import counts. Select the old
VM source with `STACK_XEC_SOURCE` to reproduce the invalid descriptor access.
The two `jit-frame-indices.py` runs add one H-descriptor rejection each.

For C fixtures, add:

```sh
export CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all'
```

The 386/ARM guards do not implement new 64-bit ABIs. MIPS, Power and SPARC
backends are outside this change.
