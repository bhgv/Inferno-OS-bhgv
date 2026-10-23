# Subnormal exponents, remainders and scale factors

Run from the repository root:

```sh
FDLIBM_CASE='ilogb fmod remainder hypot' python3 libmath/tests/ieee-boundaries.py
```

The driver compiles the production fdlibm translation units with the real
fdlibm/lib9 headers and strict UBSan. It renames exported symbols so the Python
host math functions remain independent oracles. The tests cover both signs of
subnormal significand bits, word boundaries, zero, infinity, NaN, deterministic
random binary64 representations, and identical/adjacent argument pairs.

ilogb and fmod scan unsigned significands until their top bit is set; shifting
into a signed sign bit was undefined in the old normalization loops. fmod also
uses an unsigned shift when restoring a subnormal low word. The remainder
checks call the production fmod implementation. ilogb retains its existing
zero and nonfinite sentinel values rather than adopting host sentinel macros.

hypot builds its rescaling word from the nonnegative biased exponent 1023+k.
The old k<<20 shifted a negative signed integer after scaling tiny inputs.
The floating-point algorithm and its significand splits are preserved.

fmod and remainder require exact result bits, including signed zero. ilogb uses
frexp as its finite nonzero exponent oracle. hypot permits two ULPs of difference
from the host's hypot implementation. This is functional regression coverage,
not a proof of a global error bound or correct rounding for every binary64 pair.

FDLIBM_ILOGB_SOURCE, FDLIBM_FMOD_SOURCE and FDLIBM_HYPOT_SOURCE select alternative
production sources. Every original file independently triggers a UBSan failure.
Worker processes isolate sanitizer termination; the parent removes temporary
build artifacts even when a regression fails.

The harness uses a native little-endian host, -fno-strict-aliasing for fdlibm's
word access and -fno-builtin to ensure calls enter the tested implementations.
It does not boot Inferno, execute a JIT, or validate target FPU exception flags,
endianness or ABI layout. No Dis instruction, module signature or JIT changes
are required. Run without FDLIBM_CASE for the complete current driver suite.
