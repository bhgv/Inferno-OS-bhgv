# Native-host decimal conversion and compiler constant identities

These regressions execute retained production C functions, with minimal host
headers/symbol adapters. They are not a full Inferno build or VM acceptance test.
No installed Inferno SDK, Draw service, emulator or network is needed.

```sh
python3 libmath/tests/native-dtoa-host.py
python3 limbo/tests/big-constant-host.py
```

Both scripts use CC/CFLAGS and default to C99 with UBSan. TMPDIR can select
writable temporary storage. native-dtoa-host builds the actual dtoa.c and g_fmt.c,
compares 1032 independent binary64 parser values, and verifies shortest-decimal
format/parse round trips. Cases include overflow, maximum finite, subnormals,
signed zero and randomized finite bit patterns. dtoa's strtod symbol is renamed
inside the fixture so libc and fixture plumbing are not accidentally replaced.

The big-constant fixture extracts the actual globalBconst function and supplies
small allocation/interning/Plan9-format adapters. Different negative/high-bit
constants must produce distinct twenty-character names and declarations. It
tests native formatting widths and real name truncation, not a rewritten hash.

DTOA_SOURCE and ECOM_SOURCE can select previous production sources for replay.
On an LP64 host, the previous dtoa accesses outside its double object and the
previous constant-name format truncates/merges negative values. The fixed sources
pass both regressions. Comments/literals and public exported dtoa signatures
retain their original meaning; arithmetic limbs/double halves are explicitly
32-bit, with unsigned packing of signed subtraction halves.

A separate source-built compiler/Dis integration in bhgv/flashVM checks real
compiled constants and generated Math declarations. That complementary evidence
does not establish a complete Inferno runtime/platform or decimal conformance
suite for every rounding mode and target.
