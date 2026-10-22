# Native floating comparisons and overlapping block moves

## IEEE comparisons on IA-32

The interpreter's six real branch operations use C comparisons. An unordered
comparison (either operand is NaN) must make only `bnef` branch. The IA-32 JIT
used FCOMP, FSTSW AX and SAHF, then tested ZF/CF alone. Unordered x87 results
set ZF, PF and CF together: equality and less/less-equal incorrectly branched,
and inequality incorrectly fell through.

The encoder now tests parity. Inequality has a second branch to the same Dis
target for unordered results; equality and less/less-equal skip their branch
when parity is set. Greater/greater-equal already exclude unordered results.
The existing source/middle order, branch relocation and x87 stack balance are
preserved. This change assumes masked native floating exceptions, as does the
existing x87 execution path; it does not add native trap handling.

`x86-float-branches.py` compiles the production comparison, addressing and jump
encoders and executes them in Unicorn. Scheduling is a no-op boundary. Its
42,720 cases cover all six predicates, frame/module/indirect source addressing,
frame/module middle operands, four x87 rounding modes, signed quiet/signalling
NaN bit patterns, infinities, signed zero, subnormals, finite boundaries and
deterministic random bit patterns. It checks the taken destination against
independent Python IEEE predicates and verifies that x87 TOP returns to zero.
A negative control removing the parity handling reproduces `0 == NaN`.
No target SDK, actual scheduler or native trap integration is exercised.

## Block copies must implement memmove

The VM's `movm` and `headm` use memmove. IA-32 generated CLD/REP MOVSB, while
ARM generated forward byte/word loops (and small load/store sequences). For
copies larger than the registers can capture, a destination inside and above
the source overwrites bytes before the loop reads them. These implementations
therefore differed from the interpreter for valid overlapping regions. A separate
negative control executed both previous native encoders with a 16-byte copy
to source+4 and reproduced the same repeated four-byte corruption on each.
Operand addresses in that probe were boundary stubs; the copy instructions
and loop encoding were production code.

Both backends now dispatch `movm` and `headm` through their existing VM helper
bridge with source, destination and middle operands. This reuses the production
memmove semantics, including reading the length before any writes. ARM's now
unused copy-loop generator has been removed. This can cost throughput for
small copies; no performance measurements have been made. Specialized scalar
big/real moves remain native. No new restrictions or validations of byte counts
are introduced here: negative or out-of-bounds counts remain outside this test.

`jit-memmove.py` compiles the actual dispatch blocks with a recording punt
boundary and checks both opcodes on both backends. It also executes the actual
VM `movm`/`headm` bodies in WORD32 and WORD64 host models against independent
snapshot-copy expectations. Its 1,578 cases include zero, small and large byte
counts, overlap in both directions, identical regions, adjacent/disjoint
regions, and aligned length operands inside the destination. Every byte of
the surrounding allocation is compared after each copy. It does not execute
native copy instructions: the new paths deliberately use the VM helper.
Production native bridge addressing and state restoration are exercised by
`jit-handoff.py`; actual target C ABI and a full VM boot remain outside coverage.

The source/destination objects in these host models are bounded allocated
buffers; they do not model the full GC or List ownership lifecycle.

## Running

```
python3 libinterp/tests/x86-float-branches.py
python3 libinterp/tests/jit-memmove.py
```

The first test needs Unicorn; both need a host C compiler. UBSan is enabled
by default. `CC` and `CFLAGS` can override compiler settings. These add to the
existing integer, scaled, publication, lifetime and native handoff suites.
