# Callback numeric scanner regressions

Run `python3 lib9/tests/charstod.py` from the repository root.

Both production lib9 and libkern `charstod` scanners run 1,875 cases each.
A bounded callback records every input access, the final delimiter/EOF and the
exact number of reads. Ordinary numbers, negative zero, signed NaNs and infinity
are compared with the host converter. Long mantissas/fractions/exponents test
storage bounds and complete token consumption, including exactly 399 stored
bytes plus the terminator and inputs through 2,000 digits.

Overlong tokens retain the existing NaN error result, but scanning continues
through the number and one terminating callback result. The existing grammar,
including its tab/space-only leading whitespace rule, remains in use. Signed
NaNs now follow the same optional-sign path as signed infinities. Both old
scanner bodies independently fail the regression.

Defaults use strict UBSan. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 lib9/tests/charstod.py
```

`CHARSTOD_SOURCE` selects an alternate scanner. This tests scanner control flow
and delegates decimal conversion to host strtod; it does not validate libmath
rounding, the libkern strtod wrapper, actual buffered I/O or kernel link selection.
It is not an Inferno boot or JIT test. LeakSanitizer process inspection is disabled
in the sandbox; ASan/UBSan remain enabled.
