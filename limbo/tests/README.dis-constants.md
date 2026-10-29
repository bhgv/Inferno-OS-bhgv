# Dis operand constant widths

`discon` and `disbcon` encode signed operands in one, two or four bytes. The
four-byte format reserves two tag bits and represents -536870912 through
536870911. Both writers now compare the complete host `long` with these
limits before emitting a four-byte operand. The previous negative-value
check inspected only three high bits and accepted some out-of-range LP64
values, such as -4294967297, silently emitting an unrelated small operand.

Run from the repository root:

```sh
python3 limbo/tests/dis-constants.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 limbo/tests/dis-constants.py
```

The fixture extracts both production writers and checks independent expected
wire bytes for all size transitions, signed limits and 20000 deterministic
valid operands. On LP64 it runs 40056 writer cases, including four additional
invalid wide negatives. Rejected values must emit no bytes; buffer guards
must remain intact. Output and fatal-error reporting are injected boundaries.
LeakSanitizer process inspection alone is disabled in the fixture.
`DIS_CONSTANT_SOURCE` selects an alternate `dis.c`; pre-fix sources fail the
range-rejection check. This does not run a complete Limbo compilation or
execute its output in Inferno.

## Shared encoder preparation for Dis64

Both active compiler writers now use `include/dis-operand.h`. It emits the same
shortest signed30 bytes with unsigned shifts, independent of host signed-shift
behavior. The helper bounds capacity before any write. `disword` likewise
serializes the low 32 bits with explicit unsigned shifts and byte masks.
The classic magic and operand representation remain unchanged.

The fixture now uses always-on `if` predicates, also exercises capacities 0..8,
NULL output and fixed-width word emission. On LP64 it runs 260,364 checks over
the same 20,028 values. `native-limbo.py` separately builds and invokes the real
compiler; helper tests alone do not prove a new ABI runtime.

When importing the updated `limbo/dis.c` into a selected vendor snapshot,
`include/dis-operand.h` must be imported too. Do not keep the source pin while
omitting this direct include dependency.
