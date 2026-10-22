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

A separate native executable covers 87,075 boundary/alias cases per word model.
When ASan is requested, it runs in that executable with UBSan too; the Python
oracle shared library retains UBSan and removes ASan to avoid injecting its
runtime into Python. LeakSanitizer process inspection is disabled for the sandbox.
All test state is bounded and stack/static; there is no dynamic VM heap here.
The baseline runtime reproduces invalid-shift UBSan failures.

Existing x86, ARM, MIPS, PowerPC and SPARC JITs route these opcodes through their
interpreter fallback. The tests do not execute those fallback calling conventions
or implement new 64-bit JITs. Full boot, native signal translation and platform
acceptance remain outside this fixture. Active instruction/frame layouts stay
unchanged.
