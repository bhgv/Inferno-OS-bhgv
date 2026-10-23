# Translation stream ownership and path regressions

Run `python3 limbo/tests/translation-io.py` from the repository root.
The driver extracts the production `translate` and `win2inf` functions from
both `limbo.y` and the checked-in generated parser `y.tab.c`. Their bodies
must match; each driver runs 256 combinations of output modes, requested
symbols, stream-open failures, stream-close failures and parse errors.

All output and symbol handles must be cleared between translation units,
including early failures. A requested symbol file that cannot be opened is
a compilation error. Final output or symbol flush/close errors must increment
the aggregate error count and remove every artifact created by that failed
translation. Cleanup closes every successful stream exactly once.

Drive-path normalization must preserve non-ASCII UTF-8 bytes, fold only ASCII
uppercase letters, and retain case in paths without a drive prefix.

The parser, typechecker, emitter and stream functions are explicit boundaries
here. `libbio/tests/lifecycle.py` independently executes production `Bterm`,
including native close failures and allocation/descriptor cleanup. These tests
do not replace a full compiler build or filesystem integration test.
`TRANSLATE_SOURCE`, `CC` and `CFLAGS` support baseline and sanitizer runs.
