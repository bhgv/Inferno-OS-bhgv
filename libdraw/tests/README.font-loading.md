# Font input and ownership regressions

Run from the repository root:

```
python3 libdraw/tests/font-names.py
python3 libdraw/tests/font-loading.py
cc -fsyntax-only -Werror=implicit-function-declaration -Werror=int-conversion \
  -I Linux/arm/include -I include -I emu/port \
  libdraw/buildfont.c libdraw/openfont.c libdraw/getsubfont_ft.c \
  libdraw/getsubfont.c libdraw/font.c
```

The host fixtures compile the production helper/parser/loader bodies. They use
ASan and UBSan by default. `CC` and `CFLAGS` override compiler settings.

The name fixture checks exact short-string allocations, mixed-case extensions,
UTF-8 paths, long cache keys, signed size boundaries and failed allocation.
The loading fixture uses actual public draw structures, injects failure at every
allocation in a multi-subfont description, and counts every retained owner.
It also checks rejected short-field dimensions, the 32768-subfont boundary and
file lengths that would wrap or overflow the loader's signed byte count.
Display and native file operations are stubs: these tests do not render fonts.

`FONT_NAMES_SOURCE`, `BUILDFONT_SOURCE` and `OPENFONT_SOURCE` allow comparison with
previous source versions. The previous name helpers and font parser fail these
regressions. The compile command also checks that shared TrueType declarations
prevent implicit integer return types from truncating pointers on 64-bit hosts.


## Native64 complete-token checks

`font-loading.py` now keeps all predicates active under `-DNDEBUG` and supports
`NATIVE64_ARCH`, `NATIVE64_CFLAGS`, `NATIVE64_RUNNER` and `NATIVE64_TMPDIR` in the
same manner as the port gates. Its new `font-oracles.py` uses Python integers for
C-format decimal/octal/hex tokens and independently decodes the committed font
corpus. All 224 descriptions and 4,959 full range/name records are compared.
No subfont images, rendering, VM managed calls or collector traversal are executed.
Native hosted ASan/UBSan retains ordinary leak detection as well as counted owners;
the old fixture-specific LSan-disable hook has been removed.

The `--control range`, `separator`, `unicode` and `offset` modes deliberately omit
one semantic check. Each must reach its expected numeric-status or font-admission
predicate; resource allocation, compile, signal and sanitizer failures never count
as successful omission detection. Unexpected admitted fonts are released before
the semantic stop. The standalone native ARM64/x64 job and full classic-mk target
pipeline run the normal gate plus all four controls.
