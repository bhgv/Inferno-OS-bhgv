# Module export lookup

Run `python3 libinterp/tests/module-links.py` from the repository root.

The fixture executes the real linkm, mklinkmod and linkmod bodies for 67 cases.
It tests exact case-sensitive names, similar prefixes, first/middle/last export
positions, six signatures including zero and signed extrema, missing names,
absent export tables and mismatched signatures. Sentinel fields deliberately
match the requested signature and resemble a valid frame/PC, so missing-name
rejection cannot accidentally depend on unrelated sentinel fields.

Failed direct lookups leave every destination link and the frame references
unchanged. Public linkmod tests fail both the first and a later import and
check that the partial Modlink allocation and its transferred module reference
are released once. Valid multi-import linking preserves both destinations.
The original lookup independently fails the missing-name assertion.

Strict UBSan is the default. To add ASan:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/module-links.py
```

`MODULE_LINK_SOURCE` selects an alternate link.c. LeakSanitizer process
inspection is disabled; other instrumentation remains on. Allocation,
final destruction, diagnostics and formatting are injected boundaries. The
fixture requests no MP clone or dynamic native data. It does not validate
allocation failures during linkmod/newmp, full module unloading, GC, kernel
boot or execution through a resulting native/Dis call target.
