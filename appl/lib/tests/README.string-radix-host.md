Str radix and exponent checks
============================

Both appl/lib/string.b and appl/alib/string.b now restrict decimal characters to
0..9 and alphabetic characters to the selected radix. Previously base16 accepted
punctuation such as ':' as a digit, while higher bases decoded uppercase letters
through their ASCII distance from '0': base36 'A' became 17 instead of 10.

Explicit and inferred radices now both reject values outside 2..36. In particular
an inferred 1r prefix no longer passes through the private prefix validator.
Toreal's exponent digits are accumulated in decimal for every mantissa radix,
as documented in man/2/string: 2r1e10 denotes 2 to the decimal tenth power,
not 2 to the binary tenth power. Existing mantissa, suffix and overflow policies
are outside this change.

Run `python3 -O appl/lib/tests/string-radix-host.py`. The driver extracts the
production digit body unchanged and the actual inferred-base/exponent integer
expressions from each library. These statements use a common C/Limbo integer
subset. Only signatures are adapted for host compilation; test inputs keep all
arithmetic within signed 32-bit bounds. The resulting code is checked against
an independent alphabet oracle across all radices 2..36, invalid radices, ASCII,
selected Unicode characters, uppercase/lowercase digits, punctuation and
bounded decimal exponent transitions. Visible prefix/suffix conversion examples
also exercise the production operations.

All 26,388 checks pass with UBSan/NDEBUG and under python -O. The driver is in
the host checkpoint. STRING_RADIX_SOURCE can select an older implementation for
regression replay. This is an executable integer-logic probe, not a complete
Limbo module compile/load or interpreter execution test.
