# Hexadecimal codec bounds

Run from the repository root:

```sh
python3 lib9/tests/hex-bounds.py
```

The driver compiles production lib9/u16.c into a host shared library with UBSan.
It checks 45,794 byte-oracle, capacity, guard, delimiter and extreme-count cases.
Python's hexadecimal formatting and byte parser supply the value oracles.
All byte values and deterministic variable-length vectors are included, with
uppercase/lowercase text, ignored delimiters, odd tails and arbitrary byte text.

enc16 reserves its terminator before reading an input byte. A nonpositive output
capacity causes no access, and negative counts never decrement INT_MIN. With
positive capacity and no encodable bytes, it still emits the empty C string.
dec16 accepts only 0-9, a-f and A-F as digits; other bytes retain the ignored
separator behavior, and an unmatched trailing digit produces no output byte.
This corrects the former interpretation of g-z and G-Z as hexadecimal digits.
Decoding stops at capacity without reading further pairs. All bytes beyond the
returned output (and the encoder's terminator) must retain their canary values.
