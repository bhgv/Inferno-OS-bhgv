# Constant folding, effects and powers

Run from the repository root:

```sh
python3 limbo/tests/fold-integers.py
python3 limbo/tests/fold-effects.py
python3 lib9/tests/rune-length.py
```

The arithmetic drivers default to UBSan with immediate failure on undefined
behavior. `fold-effects.py` extracts the production `foldvc`, `ipow`, `rpow`
and VM `iexpw`, `iexpl`, `iexpf` bodies. Its minimal AST supplies observable
leaf effects, then compares values and effect counts before and after folding.
It covers arbitrary integer truth values, relational results compared with
constants other than 0/1, and effectful bases raised to zero.

The same driver runs 1,336 checks in each of two VM word models (32 and 64 bits).
Power cases include unsigned modular wraparound, the minimum signed exponent,
integer reciprocal truncation, zero-base VM errors with destination retention,
real subnormal results, signed zero and infinity. Negative integer powers no
longer calculate an overflowing positive intermediate before taking a reciprocal.
Negative real powers invert the base first; this preserves subnormal answers
such as `2.0 ** -1074` and can change floating-point rounding/status flags from
the previous multiply-then-invert path. Complete OS builds and target FPU modes
are outside this host fixture.

`rune-length.py` compiles the production UTF codec and checks 1,118,210 scalar
and invalid values against the actual encoded lengths and replacement decoding.
It also checks mixed-buffer lengths and nonpositive counts. Out-of-range runes
must count as the three-byte replacement character, matching `runetochar`.
Otherwise VM string-to-byte-array conversion can expose an unwritten tail byte.

Both new fixtures fail on their pre-fix production sources. Existing ARM and
RISC-V tests remain separate; the changes retain the current Dis instruction
set and layouts for future native backends.
