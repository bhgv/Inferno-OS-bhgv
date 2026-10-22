# Native formatter bounds regression tests

Run `python3 lib9/tests/format-bounds.py` from the repository root.

This links real native lib9 formatting and rune code. Independent libc decimal
conversions verify signed extrema; a bit-scanning oracle verifies grouped binary
output for every bit position through 64 bits, including all-ones values.
Tests also check decimal width/precision overflow, an INT_MIN dynamic width,
ordinary negative widths, bounded output for INT_MAX width, and actual Rune
format dispatch through `dorfmt`.

The 144 cases run with strict UBSan by default. For ASan as well:

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 lib9/tests/format-bounds.py
```

`FORMAT_FMT_SOURCE` and `FORMAT_DOFMT_SOURCE` select alternate production files
for before/after comparisons. These tests use the host C ABI; they do not boot
Inferno or replace an emulator/kernel build. LeakSanitizer process inspection is
disabled in the sandbox; ASan/UBSan remain enabled.
