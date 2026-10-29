# Bounded GC work accounting, feedback and epoch waiting

Run `python3 libinterp/tests/gc-arithmetic.py` and
`python3 libinterp/tests/gc-idle.py` from the repository root.
GC_ARITH_SOURCE selects alternate gc.c; GC_ARITH_CASE selects integers, quanta,
color, heap, array, list or balance; GC_ARITH_WORD_BITS selects 32 and/or 64.
GC_IDLE_SOURCE selects an alternate waiter source; GC_IDLE_PLATFORM selects emu,
os or bench. Both drivers run their full current-source matrix by default.

## Production changes

The collector used signed int decrement/subtraction for remaining work. An
exhausted budget could overflow in markheap or after a nested marklist callback;
array charging could likewise revive an exhausted int budget through overflow
or native-WORD-to-int narrowing. Work charging now clamps at INT_MIN using a
wide checked difference. Zero charges and ordinary budgets retain their exact
values. Array iteration uses WORD to match its length field; scanning itself
remains nonincremental.

The net balance and inspected-block estimates now use bounded int adjustments.
These are feedback estimates, not exported exact telemetry, and saturate at the
int limits rather than changing sign through overflow. The quantum controller
computes its percentage and gain at vlong precision, clamps to Quanta..MaxQuanta
and only then narrows. Nonpositive inspected counts defensively select Quanta;
normal production counts start at one and remain positive.

The legacy signed gccolor selector now rolls from INT_MAX to 2. This preserves
the mathematically next modulo-three color and keeps all expressions used by
rootset nonnegative. Ordinary selector values are unchanged until that boundary.
The existing unsigned gcepochs counter records completed epochs independently.

Both scheduler idle waiters and the kernel GC benchmark now compare unsigned
elapsed epochs against three. They no longer add three to the signed color
selector or assume it remains monotonic across rollover. Ready-work/VM-queue
interruptions, GC-disabled behavior, scheduler ownership, timing hooks and normal
three-epoch limits retain their existing order. The public header exposes the
already existing gcepochs variable; no object layout or JIT opcode ABI changes.

Mark ordering, H handling, Setmark, pointer maps, owner reference counts, recursion
depth limits and deferred tails are retained. No allocation is added to marking.
The change does not validate arbitrary forged owner/map addresses or alter the
collector's ownership assumptions. Cyclic marking and ordinary depth behavior
remain covered by the existing packed-map graph suite.

## Verification

| Arithmetic family | Scenarios per WORD/profile |
| --- | ---: |
| Python arbitrary-precision charge/add vectors | 210 |
| Quantum feedback vectors | 90 |
| Selector rollover and actual empty-root color assignment | 64 |
| Packed descriptor/payload marks with boundary budgets and balances | 92,160 |
| Array length charging and pointer-free laziness | 108 |
| Nested list exhaustion and saturated mark balance | 2 |
| Total | 92,634 |

The mark matrix uses the real callbacks and independent expected colors, work,
balance and callback order, including repeated owners, H fields, every byte
alignment, recursion limits and complete unchanged payload/canary images.
Reference counts remain unchanged. Array and list probes exercise the real
markarray/marklist functions, including a child callback that exhausts work before
the list's own charge. The phase probe executes rootset(nil) with no module or
external roots; it verifies phase assignment rather than a full root graph walk.

Each actual idle waiter passes 980 scenarios across selector/epoch boundaries,
1..4 slices per epoch and interruption modes. The emu, os and benchmark bodies
run with controlled native/scheduler hooks. They check exact slice/epoch counts,
unsigned epoch-counter rollover, restored process state and balanced scheduler
entry/exit. These are host fixtures, not a booted kernel or emulator scheduler.

O1 ASan/UBSan, O0 ASan/UBSan and O2 UBSan pass under NDEBUG: 185,268 arithmetic
plus 2,940 idle scenarios per profile, 564,624 total. The optimized profile also
treats pointer/int conversion diagnostics as errors. LSan process inspection is
disabled; selected access/undefined instrumentation and explicit state/image
oracles remain active.

Nine before-source probes against f76cf21 reproduce runtime failures: actual
heap/array/list marks, mark balance, isolated original quantum statements and
color increment, plus each of the three actual idle waiters. The WORD64 array
probe demonstrates budget sign reversal by narrowing; the others report signed
arithmetic UB. Old helper adapters exist only in the fixture; the quantum/color
probes execute statements extracted from the original rungc body. Compile errors
are excluded from runtime regression evidence.

The existing packed GC suite also passes 2,961,056 ordinary map/graph scenarios
across WORD32/WORD64. Native bootstrap and readiness pass: real host tools,
parser/decoder regeneration and all 17 Limbo compilation units are checked.
The arithmetic driver also compiles the complete production gc.c translation
unit for each WORD model, including rungc and all helper call sites; it does not
link a complete VM. Both new drivers are registered in the core suite.

JIT availability is unchanged: AMD64/ARM64 each retain 173/173 complete JIT+C
entries, 166/173 guarded native fast paths and seven C-only entries. Full 64-bit
VM compiler/loader/frame/GC ABI and boot acceptance remain separate. Tk,
security/crypto and MIPS/SPARC/PowerPC JIT sources are unchanged.

## Benchmark waiter helper dependency

After benchmark timer subtraction moved to `tickspan`, `gc-idle.py` also extracts
that production helper from the same selected source before `idlegc`. The fixture
must not replace the helper with a test subtraction or accidentally depend on an
external device object. Older `GC_IDLE_SOURCE` parent replays without `tickspan`
retain their original complete waiter body. This changes test assembly only;
collector/scheduler/timer production behavior is unchanged.
