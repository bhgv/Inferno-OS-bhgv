# Draw image loading audit

`display_open` in `libinterp/draw.c` now validates names, dimensions and ownership
before passing decoded pixels to Draw. The following defects are fixed:

- Names shorter than four bytes no longer read before the string. Nil/empty
  names fail cleanly; ordinary short names still use the native image reader.
- The optional `:size:path` prefix requires a positive decimal size within int32.
  Missing delimiters, negative/zero sizes and overflow fail without decoding.
- PNG/SVG paths grow dynamically instead of truncating at 127 bytes.
- Pixel byte counts and strides fit the signed-int APIs before multiplication,
  allocation, scaling or upload. Zero/negative/nonfinite SVG dimensions fail.
  Nearest-neighbour PNG resizing uses bounded integer coordinates, checks
  allocation failures and preserves at least one pixel on the shorter side.
- Every decoder/error path releases its temporary image/path/rasterizer data.
  Failed upload releases the Draw image and balances display locking, including
  the already-locked case.
- Both codecs feed one straight-RGBA to premultiplied-BGRA conversion for ARGB32.
  The modified NanoSVG colour constructor is restored to its documented RGBA
  order; intermediate gradient interpolation and opacity now agree on channels.
- The PNG public header uses standard `size_t`, allowing LP64 compilation instead
  of conflicting with system headers. PNG 32-bit reads and NanoSVG alpha packing
  shift unsigned operands, avoiding signed-shift undefined behaviour.

Run from the repository root:

```sh
python3 libinterp/tests/draw-image.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  python3 libinterp/tests/draw-image.py
```

One harness extracts production loader functions and injects allocation,
codec and Draw transport failures. Another links the actual LodePNG and NanoSVG
parser/rasterizer with actual lib9 sorting/comparison. It decodes generated PNG,
SVG solid/transparent/gradient fixtures and malformed files through paths longer
than 128 bytes. Assertions inspect uploaded pixels and dimensions; explicit
allocation accounting checks cleanup. UBSan failures stop the test. ASan stack
instrumentation is enabled here; LSan remains unavailable in this sandbox.

The full draw.c translation unit passes host syntax checking. Existing NanoSVG
qsort comparator warnings remain because the project's qsort callback type
omits const; the comparator API needs a separate project-wide cleanup.

This is not a complete codec security audit or a live display integration test.
The actual codecs may allocate while decoding, before the loader can validate
reported dimensions. Full ARM/RISC-V builds and real display rendering remain
separate integration checks.
