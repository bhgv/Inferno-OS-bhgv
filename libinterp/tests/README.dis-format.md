# Safe VM instruction diagnostics

The instruction formatter now maps operand counts by opcode and prints unknown
opcodes as `op N`. It no longer indexes past the keyword array for invalid byte
opcodes. Unknown address modes have explicit `A<N>` text instead of reading an
uninitialized stack buffer. Address offsets use the complete signed WORD value,
middle immediates use the VM's signed 16-bit interpretation, and internal text
buffers use bounded formatting calls.

Run from the repository root:

```sh
python3 libinterp/tests/dis-format.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' \
  python3 libinterp/tests/dis-format.py
```

The fixture links production conv.c to the real lib9 formatting implementation.
It runs 12489 cases for each of legacy WORD64 and WORD32 layouts (24978 total):
all byte opcodes, every address-mode tag, signed offset extrema, operand counts,
all middle-operand modes, signed immediate boundaries and nil inputs. Names and
operand counts are read from the opcode definition table; expected numbers and
strings are formatted independently using libc. Every result is checked at
three destination sizes with canaries and exact truncation/termination checks.
Error-string retrieval is a boundary stub; the native formatter implementation
is exercised directly. This does not execute the disassembled instructions.

`DIS_FORMAT_SOURCE` selects an alternate conv.c. `CONV_CASES=invalid` reproduces
the pre-fix keyword-array overrun at opcode 255 under UBSan. `addresses` and
`instructions` select the offset/mode and instruction-layout tests separately.
