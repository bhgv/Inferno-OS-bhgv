# Native-kernel runtime regression tests

Run `python3 libkern/tests/native-runtime.py` from the repository root.

This runs four independent regression fixtures against libkern production
sources, rather than assuming fixes to lib9 also apply to the native kernel:

- 18,655 signed/unsigned 64-bit conversion cases, bases 2 through 36, wraparound
  compatibility, saturation above UINT64_MAX, invalid bases and end pointers.
- 1,118,210 Rune encoding/length cases, including surrogates, out-of-range values,
  exact replacement sizing, mixed output and nonpositive counts.
- 88 UTF prefix/search cases, including LONG_MIN/LONG_MAX limits, invalid search
  values, exact byte positions and existing malformed/incomplete UTF behavior.
- 144 formatting cases covering signed minima, grouped 64-bit binary output,
  decimal field overflow, dynamic widths and Rune format dispatch.

The drivers in lib9/tests accept `NATIVE_LIBRARY=libkern`. Numeric functions are
renamed only to avoid host libc symbol collisions. They retain libkern's char*
entry signatures. UTF and formatting tests compile the real kernel bodies with
native host declarations. The formatting fixture supplies existing lib9 lock,
error-format and Rune-format dispatch adapters where libkern has no standalone
file; modified fmt/dofmt and UTF bodies come from libkern.

Strict UBSan is the default. ASan can be enabled while disabling only the
sandbox-incompatible LeakSanitizer process inspection:

```
ASAN_OPTIONS=detect_leaks=0 CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libkern/tests/native-runtime.py
```

Individual source overrides documented by each fixture support independent
before/after comparisons; the eight original libkern bodies each fail their
corresponding regression. The tests do not link or boot a native kernel, model
Plan 9 varargs, execute GC/JIT code, or validate a complete 32/64-bit ABI. Syntax
checks with Inferno ARM/386 headers complement the host execution tests.
