# Quoted formatter input spans and counters

Run `python3 lib9/tests/quote-bounds.py`. The driver links actual formatting,
UTF and tokenization units from lib9 and libkern (with lib9 fallbacks for units
only present there). Default builds use fail-fast UBSan. Override CFLAGS for
ASan as well. `QUOTE_SOURCE` selects an earlier fmtquote.c for negative probes.

The fixture checks the private _quotesetup contract for finite input-byte spans
and NUL-terminated input, byte and Rune output capacities, automatic and forced
quotes, a custom punctuation callback, empty inputs, embedded apostrophes,
non-BMP scalars and every boundary through short strings. Expected counters
come from an independent scalar-width model. Finite byte inputs are also placed
in exactly sized heap allocations without NUL terminators; incomplete UTF-8
suffixes must not be decoded beyond the declared span. Rune input consumes the
same documented input-byte budget, not a Rune count.

INT_MIN input limits exercise the negative/unbounded sentinel without decrement
underflow. INT_MAX output limits exercise normal small-input operation; the
fixture does not materialize multi-gigabyte formatting output. The production
counter guards stop before any Quoteinfo field becomes unrepresentable.

Actual %q and %Q dispatch is checked with UTF-8 and Rune output, then quoted
UTF-8 is parsed back through the production tokenizer. Short output buffers
must stay NUL-terminated and preserve their trailing guard. These are native
library checks, not a full Inferno boot or native JIT execution.
