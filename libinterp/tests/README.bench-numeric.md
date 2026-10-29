# Reliable interpreter/JIT timing arithmetic and reports

OS benchmark measurement previously squared signed tick counts and accumulated
signed sums. A single sample of 3,037,000,500 ticks overflowed its square; merging
two valid 3-billion-tick samples overflowed the sum of squares. Empty deviation
calculation divided by zero. The integer variance formula also reported a
standard deviation of 22 ticks for the two samples 1000 and 1001, whose truncated
population deviation is zero.

`Stat` now stores an exact first-sample origin, count and extrema, a floating
mean offset and a centered sum of squared deviations. Welford updates and
population merges avoid integer sums/squares and catastrophic subtraction of raw
second moments. Centering also retains small spreads near the signed timestamp
limits. Empty/singleton/constant populations produce zero deviation. Counts are
bounded before increment/merge, self-merges snapshot their source, and final
floating-to-signed conversions are bounded. Means remain floating estimates;
this is diagnostic statistical storage, not an exact integer-sum facility.

Per-instruction floating updates mask traps and select nearest rounding through
the existing platform FPcontrol/FPstatus API, then restore sticky flags and
controls. That bookkeeping occurs after the end timestamp, outside the measured
opcode interval. Report-time floating operations retain the existing reporting
context; this change does not establish a general floating-environment barrier
for all benchmark commands.

`tickspan` subtracts unsigned counter bit patterns, accepting small intervals
across the sign bit and full-width rollover without signed overflow. Backward
or stale intervals exceeding INT64_MAX are discarded as zero. C-step, compiled
hook and GC timing use it; GC totals saturate before addition can overflow.
The three low-32-bit calibration/overhead/operation loops now accumulate in
64-bit unsigned totals and explicitly truncate timer reads to 32 bits before
subtraction. This assumes each measured low-word interval spans less than one
complete hardware low-word cycle; it does not detect multiple wraps.

Calibration uses a wide ticks-per-millisecond value. Microsecond conversion no
longer truncates that frequency by dividing it by 1000 first. Uncalibrated
conversion is defined, reset still captures its origin before calibration, and
integer results saturate before conversion. Microsecond endpoints use exact
quotient/remainder arithmetic, including a bounded binary scaling path when
remainder*1000 would overflow; they do not alter the VM floating environment. Seconds divide milliseconds by 1000
rather than multiplying. Unit boundaries are consistent, negative formatted
values retain their sign, and uncalibrated strings display raw ticks.

Report reads check length, buffer state and logical offsets before forming a
pointer. The microsecond endpoint handles zero calibration, unsigned absolute
counter values, conversion limits, a visible zero string, and successive file
read offsets. No public module, JIT code, frame or Dis instruction ABI changes.
The private benchmark record layout changes and must be rebuilt; its make
rule now tracks the floating API header.

## Verification

```
python3 libinterp/tests/bench-numeric.py
CFLAGS='-O2 -g -DNDEBUG -frounding-math -fsanitize=address,undefined,float-cast-overflow -fno-sanitize-recover=all' python3 libinterp/tests/bench-numeric.py
python3 libinterp/tests/instrument-loops.py
python3 libinterp/tests/jit-dispatch-native.py
python3 libinterp/tests/arm64-dispatch-compiler.py
python3 libinterp/tests/jit-readiness.py --check
```

The new native fixture passes **19,477 predicates** and borrows actual statistical, calibration, overhead,
operation-sampling, formatting, elapsed-time and reader bodies. It checks every
split of deterministic populations against an independent centered long-double
two-pass oracle, including empty populations, high origins, opposite signed
extremes, random values, self-merges and count saturation. Host FP adapters keep
control and status restoration independent, use real fenv state, and test four
rounding modes, sticky flags and enabled traps. An independent unsigned
128-bit oracle checks 4,096 full-width scaling inputs; this wide type is used
only by the test, not the production implementation. Scheduler/timer/directory/format
adapters are controlled fixtures, not a complete OS runtime or the platform's
native FPcontrol implementation. The fixture passes at O1 and O2 with ASan,
UBSan, float-cast-overflow and NDEBUG. Leak scanning is disabled because this
sandbox cannot enumerate process tasks; no production/fixture stack checks are
disabled.

Nine parent selections fail during execution: `statistics`, `large`, `merging`,
`spread`, `formatting`, `elapsed`, `samplers`, `reading`, `clockreading`. Replay
with `BENCH_NUMERIC_SOURCE=/path/to/51d18ea-devbench.c` and
`BENCH_NUMERIC_CASE=<selection>`. The parent mean adapter uses its actual report
expression; legacy formatter spellings are mapped at the host API boundary.
The parent statistics case exposes empty division, and independent large/merge
cases expose signed overflow. These are runtime failures, not build failures.

The instrumented-loop fixture adds real `bxec` and compiled pre/post-hook clock
matrices across both timestamp boundaries. WORD32/WORD64 each pass 3,580
benchmark parent predicates, and the unchanged debugger/coverage variants keep
2,220/2,516 per variant/model: 26,104 combined parent predicates. Child error
checks are additional. `INSTRUMENT_CASE=clock-c|clock-jit`,
`INSTRUMENT_KIND=bench`, `INSTRUMENT_WORD_BITS=32` and the same parent
`INSTRUMENT_SOURCE` independently reproduce signed subtraction overflow in both
actual timing routes. Its parent replay uses current private statistics types
and accumulator, isolating the old timing bodies.

Existing native C/RX dispatch passes 177,152 routes, 6,183 compiler cases and 295
integration checks per WORD model. ARM64 compiler preflight passes 6,215 per
model. Fresh availability remains 173/173 complete JIT+C operations and 166/173
native fast paths on AMD64 and ARM64. No new opcode is added, and no complete
64-bit OS boot or physical ARM64 run is established. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources remain unchanged.
