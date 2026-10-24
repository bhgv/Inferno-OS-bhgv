# Fixed-point instruction arithmetic

Run from the repository root:

```sh
python3 libinterp/tests/fixed-ops.py
python3 limbo/tests/fixed-ops.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libinterp/tests/fixed-ops.py
```

The runtime driver compiles all nine production mulx/divx/cvtxx instruction
variants and their shared arithmetic helpers in WORD32 and WORD64 scalar models.
Each model checks 37,800 cases against an independent Python arbitrary-precision
oracle. Cases include full-word extrema, random bit patterns, signed operands
and coefficients, every rounding-flag combination valid for the chosen shift,
large/minimum counts, zero divisors, and all source/destination alias choices.
VM errors must leave the actual destination unchanged. A small setjmp boundary
models error delivery; this is not execution through the complete VM or JIT ABI.

Fixed intermediates now use unsigned bit arithmetic for products, left shifts,
rounding additions and corrections. Right shifts retain the host's arithmetic
signed-right-shift convention. Counts are checked before evaluated shifts, and
right-shift rounding masks require negative counts. Division reports exZdiv
rather than a native fault; zero-numerator shortcuts cannot hide zero divisors.
The signed minimum divided by -1 retains its modular bit pattern without a host
overflow instruction. This does not establish a new language-level contract for
overflowing fixed arithmetic; it makes the existing bit behavior independent of
C signed-overflow optimizations.

A separate native executable covers 1,625,400 boundary/alignment/alias cases per word model. It varies frame, source, middle and destination byte offsets and includes partial destination overlap and both frame temporary slots.
When ASan is requested, it runs in that executable with UBSan too; the Python
oracle shared library retains UBSan and removes ASan to avoid injecting its
runtime into Python. LeakSanitizer process inspection is disabled for the sandbox.
All test state is bounded and stack/static; there is no dynamic VM heap here.
The baseline runtime reproduces invalid-shift UBSan failures.

The scalar fixture now reads actual Stmp/Dtmp byte slots in an explicit frame
buffer. The Python arbitrary-precision oracle still checks 37,800 cases per word
model, now with varying source/middle/destination/frame alignment. Full byte
snapshots verify that errors preserve storage and successful instructions modify
only the destination word. The new 64-bit decoded-operation JITs implement mulx
and divx as described in README.fixed-frame.md; the remaining fixed variants and
legacy native-PC backends keep their existing C fallback. Frame/call ABI layout
migration and full target boots remain outside these fixtures.
