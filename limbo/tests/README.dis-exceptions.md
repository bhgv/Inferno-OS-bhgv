# Packed exception-table counts

The exception metadata writer packs the total label count into 16 low bits and
the typed-label count above it. The result is a positive signed Dis operand,
so at most 13 bits remain for the typed count. `disexc` now checks every reachable
record before writing anything: 0..65535 labels, 0..8191 typed labels, and no more
typed labels than total labels. This prevents signed-shift undefined behavior
and silently changing the decoded counts. Unreachable records are still skipped.

Run from the repository root:

```sh
python3 limbo/tests/dis-exceptions.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 limbo/tests/dis-exceptions.py
```

The fixture executes the production writer for 130 cases. It checks both sides
of the 8191/8192 and 65535/65536 transitions, negative/extreme counts, reachability,
missing case metadata, and a later invalid record after a valid one. Invalid
input must emit no header, fields or strings. Valid packed fields are decoded
independently and compared with the requested counts, including maximum values.
Formatting, byte-output and diagnostics are boundary stubs. This is a serializer
fixture, not a complete Limbo compilation or an exception-handler runtime test.

`DIS_EXCEPTION_SOURCE` selects an alternate `dis.c`. Pre-fix sources fail the
rejection/no-output checks or trigger UBSan on negative packed-count shifts.
