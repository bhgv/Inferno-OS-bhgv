# Multiprecision conversion and base32 regressions

Run from the repository root:

```sh
python3 lib9/tests/base32.py
python3 libmp/tests/conversions.py
python3 libmp/tests/text.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' python3 libmp/tests/conversions.py
```

All drivers default to UBSan with immediate failure. New fixtures fail against
the pre-fix production sources.

`base32.py` compiles the actual lib9 codec. It compares 23,893 encoding, decoding,
capacity and guard-byte cases with Python base64's base32 bit grouping translated
to Inferno's alphabet. All tail lengths, malformed groups, invalid characters,
exact-size buffers, empty input and extreme rejected counts are covered. Codec
failures are checked for leaving the destination unchanged.

`conversions.py` includes production signed/unsigned scalar and endian import
functions, plus byte exporters. The real `mp.h` defines the structures; an
exact-capacity allocator models the mpnew/mpbits contracts. There are 4,018
scalar/byte cases in each of the 32-bit and 64-bit digit models, including reused
negative destinations, stale zero storage, minimum signed values, full-width
limbs, saturation and empty NULL imports. Both models pass AddressSanitizer.
LeakSanitizer process inspection is disabled in that executable because the
sandbox hides `/proc`.

`text.py` links production portable mp allocation, arithmetic, division, shifts,
parsing and formatting code with lib9's codecs. Python integers supply the value
oracle. It checks 336 decimal/hex/base32/base64 round trips, large right shifts,
low-set-bit positions, exact base32 output and balanced temporary ownership.
It also compares 480 modular power cases with Python pow, including zero
exponents, modulus one and output aliasing each input, and checks the minimum
negative native shift without signed negation overflow.
Ordinary allocation failures are injected into output buffers and parsers;
failures in fatal mpnew/mpbits allocation paths are outside this fixture.

The production fixes include widening before upper-limb shifts, unsigned
magnitudes for signed minima, full unsigned saturation, correct scalar allocation
bit counts, zero handling, normalized unsigned byte imports, empty input without
a read, unsigned quotient/low-bit masks, and checking large right shifts before
computing allocation sizes.

The base32 codec now emits complete partial groups and validates lengths,
characters and exact capacities before decoding. libmp's text path no longer
pads the integer's byte representation to a multiple of five before encoding:
those trailing bytes changed its numeric value. Base32 output for such values
therefore changes to the exact encoding of their magnitude. The parser uses the
base32 alphabet and rejects incomplete groups instead of converting an error
length into an unsigned byte count. Failed parsing releases only destinations
allocated internally. Hex formatting no longer forms a pointer before the digit
array when formatting zero.

These are host regression tests, not a complete Inferno build or a JIT backend
implementation. The 64-bit digit fixture checks conversion portability; active
VM layouts and the repository's current 32-bit mpdigit typedefs are unchanged.
