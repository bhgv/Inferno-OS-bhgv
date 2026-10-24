# Full string representation comparisons and native-width heap queries

AMD64 and ARM64 now execute all six string comparison branches for byte/byte,
Rune/Rune and mixed byte/Rune operands. Explicit character lengths preserve
embedded NULs and prefix ordering. Byte loads are unsigned; Rune loads retain
the existing C comparator's signed int keys, including non-Unicode 32-bit Rune
patterns. Nil is empty. C-null owners and INT_MIN lengths still return to the
C error path before changing VM memory or PC. Identical owners compare equal
without reading the payload, including very large positive/negative lengths.
A false predicate never reads its native branch-target slot.

The string headers retain a one/four-byte stride in caller-saved registers.
ARM64 shares a side-effect-free fallback return and uses x1/x2 for strides in
addition to its existing volatile registers. Both paths remain native-stack,
helper-call, allocation and FP-state free. Generated sizes are 58 four-byte
units on AMD64 and 62 instructions on ARM64, within the unchanged private
64-unit buffers. Public REG, Frame, String, pointer maps and the Dis format
are unchanged. Opcode availability stays 109/64 for both priority backends:
this removes a representation restriction from six existing paths.

```
LEAF64_CASE=bytebranch python3 libinterp/tests/leaf64-jit.py
STRING_BRANCH_NATIVE=1 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/string-branches.py
LEAF_RUNTIME_BACKEND=arm64 LEAF_WORD_BITS=32 python3 libinterp/tests/leaf64-runtime.py
LEAF_RUNTIME_BACKEND=arm64 python3 libinterp/tests/leaf64-runtime.py
```

The shared Unicorn suite has 29170 checks. It covers mixed representations,
Unicode limits, signed Rune keys, equality/inequality/prefixes, nil/null,
INT_MIN metadata, alias identities, exact memory images, false unmapped targets
and bounded code emission/disassembly. Read hooks reject character accesses
past each declared byte/Rune payload boundary. The actual native AMD64 fixture
runs 833028 native/C/oracle scenarios per WORD model (1666056 total), including
all byte pairs, independent frame-slot alignments and mixed Rune-key tests.
Native runtime matrices pass with ASan/UBSan for both WORD models. ARM64 is
executed in Unicorn and checked for code publication/lifetime on the host;
physical ARM64 system acceptance remains separate.

## Process heap-query C implementation

Both emu/os `progheap` readers now use byte copies for four-byte aligned Dis
slots. Big and real formats read exactly sizeof(LONG)/sizeof(REAL), avoiding
two native WORD loads. WORD and array/channel lengths are formatted without
int vararg truncation. Reference formats advance by native pointer width;
string and channel formats now advance rather than repeating the same slot.
Nil Modlink formatting avoids dereferencing H. C-null typed owners, invalid
string lengths and C-null channel buffers are rejected before their metadata
is inspected. Raw pointer queries still permit the numeric null pointer value.

Pagination retains the full ulong offset in an unsigned accumulator and never
passes a nonpositive output capacity into formatting after a string fills the
buffer. Empty destinations return before reading the query. These changes
preserve the existing record formats and short-output conventions rather than
introducing a new debugger protocol.

Instruction queries treat `pc+module.In` addresses as Dis instruction indices,
advance one instruction per record and check the module program/count before
constructing a pointer. Native-compiled legacy modules are rejected because
their program storage is not a Dis instruction array; current 64-bit leaf-JIT
modules keep `compiled == 0` and retain their Dis program, so they remain
readable. Native machine-code disassembly requires a separate mapped-PC path.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/heap-query.py
HEAP_QUERY_CASE=exact CFLAGS='-O0 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/heap-query.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

The new driver runs actual emu/os readers with real lib9 formatters and
string2c, native heap/type declarations and independent record/substring
oracles. There are 306 cases per environment/WORD model (1224 total). Cases
cover all data formats, exact eight-byte allocations, 0/4-byte slot alignment,
native reference arrays, nil and malformed owners, large WORD values,
zero/short destinations, offsets beyond 32 bits, string/channel progression,
consecutive instruction indices and compiled/bounds rejection. The instruction
formatter validates selected instruction pointers while outputting a simple
fixture label; native instruction pretty-printing is outside this fixture.

`HEAP_QUERY_SOURCE` selects another devprog.c revision; `HEAP_QUERY_CASE`
selects `exact`, `nilmodule`, `offset`, `valid`, `code` or `invalid`. On the
parent source, `exact` at -O0 reproduces an ASan heap over-read from the second
WORD load; optimization can remove that unused union word load at -O1.
`nilmodule` reproduces a UBSan invalid Modlink access, and `offset` fails the
independent large-offset oracle. The corrected exact-width reader passes at
both optimization levels with instrumentation enabled.

## Coverage after this series

Of 173 active Dis commands, AMD64 and ARM64 each have 109 native paths and
64 interpreter-only commands. RV64+M remains 103/70, RV64+Zmmul 90/83, and
RV64I 87/86. Other native operations can still have operand restrictions.
Complex C helpers remain shared; full native-block execution and a coordinated
64-bit compiler/loader/frame/GC pointer ABI are separate work. Tk, security/
crypto and the MIPS/SPARC/PowerPC JITs are not changed by this series.
