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
