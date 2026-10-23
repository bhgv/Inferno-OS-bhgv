# Real-to-integer runtime bounds

Run from the repository root:

```sh
python3 libinterp/tests/real-integers.py
CFLAGS='-O1 -g -fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer' python3 libinterp/tests/real-integers.py
```

The driver extracts production cvtfw/cvtfl and executes 40,264 checks in each
WORD32/WORD64 scalar model. An independent long-double truncation oracle checks
representability of the rounded double. Cases cover ties, both zero signs,
subnormals, signed bounds and their nextafter neighbors, NaN/infinities, and
10,000 deterministic arbitrary double bit patterns. Outputs use separate storage
or alias the input; rejected conversions preserve every destination byte.
The original sources reproduce float-cast-overflow under UBSan.

Both operations retain the existing add/subtract-0.5 rounding expression, then
reject NaN, infinity and values whose truncated result cannot fit. exInval is
raised before casting or publishing a destination. The 32-bit lower check is
strictly greater than -2147483649, since truncation towards zero permits rounded
fractional values below INT_MIN. For 64-bit limits, double spacing permits an
inclusive -2^63 lower bound and requires an exclusive +2^63 upper bound.

Limbo already declines to fold unrepresentable real/integer constants. ARM and
386 JITs already punt these conversions to the checked runtime implementation;
no native instruction selection or Dis layout changes are needed. Full VM error
unwinding, native JIT execution of these operations and hardware rounding-mode
integration are outside this scalar harness. WORD32 keeps native host pointers;
it does not establish a complete 32-bit ABI or a new 64-bit JIT backend.
