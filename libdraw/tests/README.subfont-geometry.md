# Native subfont geometry

Run `python3 libdraw/tests/subfont-geometry.py` from a checkout. The fixture
compiles actual Draw declarations, `stringsubfont.c`, `lib9/rune.c` and Point/
Rectangle constructors. Only the void Draw boundary is observed. No fabricated
VM Heap, renderer, device, collector or guest call is used.

213,420 active predicates compare every signed-byte bearing and four widths at
64 pairs of Int32 placement boundaries against wide independent arithmetic.
Additional cases cover real UTF-8 glyphs, absent/truncated characters, malformed
adjacent/vertical metrics, nil input, a successfully drawn prefix, pen-only
overflow and a real 8,421,506-byte text whose width exceeds Int32. Measurement
returns the representable prefix; placement stops before any invalid rectangle
or next pen position reaches Draw. Successful transport is not observable through
the existing void API, so a returned pen is not an acknowledgement from a device.

`--char-model signed` and `--char-model unsigned` exercise the same cases with
both compiler char models. `--control metrics`, `--control placement` and
`--control width` must fail at their named semantic predicate. Width control
returns an incorrect representable result rather than provoking signed overflow.
Compiler errors, signals, timeouts and sanitizer failures never satisfy controls.

`NATIVE64_ARCH` supports amd64, arm64 and riscv64 and checks actual compiler
identity. `CC`, `NATIVE64_CFLAGS`, `NATIVE64_RUNNER` and `NATIVE64_TMPDIR` select
the toolchain, sanitizers and target runner. Default local flags are O3 with
nonrecovering UBSan. Focused hosted ARM64/x64 jobs use ASan+UBSan with ordinary
leak detection. The full native64 port pipeline also runs all modes/controls.

This is bounded native C-consumer evidence. It does not enable managed Draw,
prove arbitrary pointer provenance/capacity, solve synchronization or establish
Android rendering/scale/startup correctness. Callers still own the font/image
lifetimes and the advertised `n+1` metrics array.
