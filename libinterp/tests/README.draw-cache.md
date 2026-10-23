# Draw cache and window-array regressions

Run from the repository root:

```
python3 libinterp/tests/draw-cache.py
python3 libinterp/tests/draw-windows.py
```

Both fixtures compile production function bodies with the repository's public
structures. ASan and UBSan are enabled by default; CC and CFLAGS can override
the compiler. Native locks, allocation failures and server calls are controlled
boundary stubs; no running Draw server is required.

The cache fixture uses the actual BIHASH constant and a wider unsigned oracle.
It covers long/high-byte names with signed and unsigned char, independent
allocation failures for records and names, duplicates, display-specific
collisions, removal and balanced locking. A failed name copy must never publish
a record or retain its allocation.

The window fixture covers nil/empty arrays, unrepresentable allocation counts,
all nil-element masks in an eight-window group, failed allocation, wrong-screen
images and dead image wrappers. Successful calls must retain the exact order
of the non-nil windows and flush while holding the display lock.

DRAW_CACHE_SOURCE and DRAW_WINDOWS_SOURCE can select earlier draw.c versions;
the previous implementations fail these tests. These are host helper tests,
not an end-to-end VM or display-server build.
