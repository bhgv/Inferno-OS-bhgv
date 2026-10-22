# Integer execution, byte division and numeric input

Run from the repository root:

```sh
python3 libinterp/tests/integer-ops.py
python3 limbo/tests/fold-integers.py
python3 limbo/tests/numeric-lexer.py
python3 lib9/tests/integer-conversion.py
python3 libbio/tests/runes.py
python3 libinterp/tests/386-byte-div.py
```

The x86 execution test needs Unicorn and Capstone. It compiles the production
emitter constants and helpers and executes all nonzero byte dividend/divisor
pairs for both quotient and remainder. Dirty upper register bits reproduce the
old byte IDIV failure. The 130,562 cases also check native zero-divisor faults
and preservation of the VM frame/module registers. Address-mode decoding and
native signal-to-VM-error translation are outside this helper fixture.

`integer-ops.py` extracts the real VM word/big addition, subtraction,
multiplication and left-shift operations. UBSan verifies 31,104 checks with
32-bit words and 39,552 checks with legacy 64-bit words. Tests cover sign-bit
boundaries, wraparound and source/destination aliases. Only valid shift counts
are covered. Limbo documents overflowing arithmetic as undefined; these changes
make existing modular machine behavior independent of C signed-overflow
optimization without defining a new language-level overflow policy.

The compiler folder now handles a divisor of -1 without executing a host signed
MIN/-1 divide instruction. Its 398 cases include the minimum big quotient and
zero remainder. This does not change native JIT handling of undefined signed
division overflow.

The numeric scanner fixture extracts production numeric parsing functions. It
covers EOF after a complete token, malformed exponent/radix tails, exact token
buffer limits and radix markers at the buffer boundary. Overlong tokens are
fully consumed and diagnosed before any truncated prefix is decoded. Integer
literal accumulation retains its modular bit-pattern behavior without signed
C overflow.

The integer library fixture covers 18,655 base/sign/full-width cases plus
saturation and end-pointer checks. Unsigned conversion uses all 64 result bits,
and overflows saturate to UINT64_MAX. Signed conversion retains legacy modular
behavior. Invalid bases, including base 1, consume no input.

The buffered rune fixture uses production Bgetc, Bungetc, Bgetrune, Bungetrune,
Bputrune and the UTF codec with a controlled short-read transport. It checks 110
cases across seven refill sizes: encoded U+FFFD, invalid prefixes, truncated
suffixes, unget after replacement/EOF, and ASCII/multibyte write failures.
Malformed UTF is replaced one byte at a time, preserving subsequent bytes.
The writer transport is stubbed; real files and OS I/O faults are outside scope.

The scanner and buffered input fixtures also pass AddressSanitizer. LeakSanitizer
process inspection is disabled in these fixtures because the sandbox hides
`/proc`; neither fixture allocates dynamic storage. New tests fail against their
pre-fix production implementations. No complete Inferno boot is implied.
