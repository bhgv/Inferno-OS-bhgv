# Explicit native64 records and an actual Math consumer

This step connects the real Limbo `-H` producer to the complete owning
`libinterp/math.c` translation unit and selected actual C entry points. It does
not register a Dis64 module, extend the shared wire/signature format, or switch
the installed native ABI. The ordinary Math build still includes runt.h and
mathmod.h. Complex implementations remain C.

## Generated records

Define `LIMBO_NATIVE64_ENTRIES` before an explicit generated header to request
private `LimboNative64Entry` records. Each non-variadic function has an immutable
record and a separate complete map array:

- source function name and actual C function pointer;
- existing Limbo signature hash, explicitly named `legacy_signature`;
- full fixed-frame size, map byte count and borrowed immutable map pointer.

These are individually addressable records, not an installed runtime table.
The next [private table/binding checkpoint](dis64-native64-binding.md) adds
callback-free generated specification tables and matching before invocation.
There is no conversion to Runtab, no map[16] copy and no callable-module
admission. The old hash is useful for this owning compiler/C consistency check;
it is not a structural Dis64 signature or agreement on reference calls.
Variadic prefixes keep their existing declarations/maps and marker but produce
no entry record. A150-reference fixture retains all20 map bytes in its record,
including the suffix beyond the old16-byte Runtab capacity. The regression
checks those bytes against the separately emitted full map. It does not claim
a tested legacy-table adapter: such an adapter has not been introduced.

Records are opt-in so geometry-only headers do not acquire function-address
link dependencies. The guarded record type can coexist across separately
generated Sys/Math headers. Ordinary name/shape preflight still applies.

## Actual consumer and registration boundary

The explicit compiler macro `DIS_NATIVE64_MATH_HEADER` selects a generated
header for math.c. DIS_WORD32=1 is required; the ordinary include path and table
are retained when the macro is absent. With this selector mathmodinit reports
an unsupported experimental registration and returns before any legacy
builtinmod/table/formatter operation. The fixture observes this refusal.
No normal build target sets the selector.

The test compiles the complete actual math.c with the generated Math header,
then binds the selected generated records against the complete generated
Math specification table and links real Math_bits32real,
Math_realbits32, Math_bits64real and Math_realbits64 implementations. Function/
data sections and linker garbage collection discard unexercised routines and
their platform services. This is not a link/run of every Math implementation.
No replacement numerical implementation or extracted function body is used.
The fixture supplies only the error observer needed for registration refusal.

Both WORD32 and WORD32+file-loader selections execute192 calls each, across
eight result-address alignments. Independent bit representations check signed
zero, positive/negative finite values and infinities. Return canaries check the
binary32 conversion writes; the binary64 paths check exact result bits. Generated
records are checked against actual function addresses, independent legacy hash
constants, actual frame sizes and scalar-only full maps. The realbits64 C code
now copies the unsigned bit representation into its signed result object via
memmove, avoiding an implementation-defined unsigned-to-signed value conversion.

```
python3 utils/tests/native64-layout.py
NATIVE64_CFLAGS='-O3 -g -DNDEBUG -fno-strict-aliasing -fsanitize=undefined -fno-sanitize-recover=all' python3 utils/tests/native64-layout.py
```

The layout driver builds a fresh compiler and invokes native64-math.py with that
exact executable. The Math driver can also build independently, or explicitly
use NATIVE64_LIMBO. Both actual objects and probes use the requested sanitizer
flags. The layout suite additionally checks the20-byte entry and absence of a
variadic record; the Math driver verifies legacy scalar selection is rejected
at compilation with the intended diagnostic.

This closes a scalar producer/record/actual-C-consumer checkpoint. Reference
arguments/results, allocation, forced GC during result ownership transfer,
caller/result/temporary roots, unwind, runtime registration and full new-format
execution remain separate gates. No ARM64/RV64 hardware execution is claimed.
