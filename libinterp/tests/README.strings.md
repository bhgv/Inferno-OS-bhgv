# String and compiler integer regressions

Run from the repository root:

```sh
python3 libinterp/tests/strings.py
python3 limbo/tests/fold-integers.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libinterp/tests/strings.py
```

Both drivers compile the actual production function bodies. The string fixture
uses the real VM headers with a bounded allocator, exact capacities, payload
guards and reference accounting. It covers self append with and without spare
capacity in ASCII/rune storage, nil insertion, shared-destination bounds failures,
allocation-size rejection, negative rune-count compatibility, UTF buffer-size
rejection and signed big conversion extrema.

The compiler fixture supplies a minimal AST and diagnostics around `foldc`,
checking 398 shift, sign-extension, width and wraparound cases under UBSan by
default. These tests do not build the complete Limbo compiler or boot Inferno.

The tests fail against the pre-fix production sources: self append changes bytes
beyond the result, and big right-shift folding shifts an `int` mask by 63 bits.
The string harness disables LeakSanitizer's process inspection because this
sandbox does not expose `/proc`; its own allocation/reference checks must balance.

These changes preserve the current Dis layouts and instruction set. They do not
introduce a new pointer-width ABI or implement an x86-64/ARM64 JIT. Keeping the
integer folding rules independent of host `int` is necessary for those future
backends and the existing RV64 backend.
