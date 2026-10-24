# Shift and division operand alignment

All eight integer shifts and six integer division/remainder handlers now capture
unaligned inputs with memmove and publish only after successful validation.
Shift counts are validated before reading the middle operand. Byte zero divisors
are checked before reading the dividend, as before. Signed word/big division
retains minimum/-1 wrapping and the corresponding zero remainder.

`python libinterp/tests/shift-div.py` runs both native-pointer WORD models under
UBSan by default. With
`CFLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"`, both
models also pass ASan with stack instrumentation enabled. Only unsupported leak
process inspection is disabled.

- WORD64: 4,632,576 valid boundary/alignment/alias cases.
- WORD32: 3,634,176 valid boundary/alignment/alias cases.
- Fourteen isolated exception processes per model check unchanged destination/PC
  and VM error identity. Invalid shifts and byte zero division use inaccessible
  middle operands to check validation order.

Source and middle alignment vary independently across offsets 0 through 7;
destinations include every byte alignment, exact aliases and partial overlaps.
The oracle uses byte snapshots and separate signed/unsigned numeric expressions,
including minimum/-1 without invoking host C overflow.

`SHIFT_DIV_SOURCE=/path/to/xec.c` tests another implementation; parent 1223aa9
fails on an unaligned shift-count WORD load. `SHIFT_DIV_CASE=div` starts at the
division cases and reproduces an unaligned WORD result store on the same parent.

The production x86-64 `leaf64-runtime.py` harness publishes real modules and
executes the same valid matrices through native entries and C handlers, comparing
whole-buffer and register effects. It also tests the expanded ICVTXX native range
-63..63 at all byte/overlap placements. Both word models pass ASan/UBSan. The
focused emulator run `LEAF64_CASE=fixed python libinterp/tests/leaf64-jit.py`
passes 44,154 checks across x86-64, ARM64 and RV64. No new helper calls, native
frames or GC maps are introduced. RV32 still declines ICVTXX; RV64I suffices.

The core environment suite passes 34 host drivers. ARM64/RV64 compiler lifetime
checks run on the host and target instructions run under Unicorn. Hardware
ARM64/RV64 execution and complete Inferno boots remain untested.
