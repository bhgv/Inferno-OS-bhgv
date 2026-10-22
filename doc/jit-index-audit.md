# Byte operands, integer branches and ARM indexing

## Immediate byte conversion

ARM's shared operand loader used the entire 32-bit immediate even for Ldb.
Memory Ldb operands were zero-extended bytes, but `cvtbw $256` consequently
produced 256 instead of zero, and negative immediates could remain negative.
Immediate Ldb operands now use the low eight bits before constant emission.
Other operand widths and the active WORD ABI are unchanged.

## String bounds and emitter arithmetic

With bounds checking enabled, ARM copied String.len into a temporary using
ORR without setting flags, then conditionally negated it using LT. Those flags
came from the earlier pointer/nil comparison, rather than the string length.
Rune strings store a negative length. Failing to take its magnitude could
make the unsigned bounds comparison accept an out-of-range index. A high
object pointer could similarly cause an ASCII length to be negated.

The copy now uses ADDS with zero: its N flag follows the length and V is
cleared. The subsequent signed test therefore selects the correct magnitude
independently of pointer values and earlier arithmetic flags. The later
representation-selection comparison is unchanged. This is the existing
optional bflag bounds path; disabling bflag still omits bounds enforcement.

Two address-emission expressions also shifted signed negative values: the
string immediate fast-path selection and fixed-width array immediate scaling.
Both now shift an unsigned target-width value. Invalid indices still reach
the existing runtime bounds check; the C compiler no longer encounters
undefined arithmetic while emitting their code. Bytecode validation or the
native emitter must not assume that a runtime-invalid index was filtered out.

## Native IA-32 byte comparisons with immediate sources

The IA-32 byte comparison encoder rejected an immediate source with urk(),
although the interpreter accepts it. Such an instruction could prevent native
compilation of its module. It now compares AL against the low byte of the
immediate and reverses the unsigned relation to preserve source/middle order.
The shared relation-reversal helper now includes unsigned comparisons.
Equality/inequality and the signed-word mappings retain their behavior.

## Executed checks

```
python3 libinterp/tests/arm-indexing.py
python3 libinterp/tests/x86-integer-branches.py
```

Both tests compile production address/constant/branch encoders and execute
native code in Unicorn, with UBSan enabled for the host emitter by default.
CC and CFLAGS can override the host compiler settings.

- arm-indexing: 3,452 checks of byte-to-word conversions, ASCII/Rune indexing,
  fixed-width and general array addresses, frame/module/indirect operands,
  immediate indices, nil references, positive/high-bit object pointers and
  earlier condition flags. It covers negative/out-of-range indices, both
  bounds-check settings for nil, and valid unchecked strings. It checks
  two-pass sizes, error destination preservation and the surrounding frame/
  module buffers. Layouts are projected ARM32 String/Array/Type/REG models;
  real constant pools, conditional calls, loads and stores are emitted.
  Error functions are recording stop boundaries, not the VM handler/longjmp.
- x86-integer-branches: 34,176 checks across unsigned byte, signed word and
  signed big values, all six relations, direct/module/indirect operands and
  permitted immediate forms. Native relative target relocation is retained;
  the scheduling boundary is a no-op. Cases include high/low-half boundaries,
  sign transitions and deterministic random operands. An independent Python
  integer oracle checks which destination is reached.

Separate negative controls restored each old ARM expression: byte conversion
mismatched its oracle, string bounds failed to report an invalid index, and
UBSan stopped on both signed shifts. Restoring the old x86 byte encoder
aborted at the previously unsupported immediate-source case. These controls
were temporary experiments; the normal suites use the current source only.

All 32 libinterp Python suites pass. Full xec/IA-32/ARM syntax checks pass
with Linux headers, including the legacy ARM architecture profile. The new
ARM indexing suite also passes all 3,452 cases with __ARM_ARCH=4.

These checks do not execute the target C runtime, actual native signal/GC/
exception integration or a complete OS boot. They do not validate arbitrary
corrupt String/Array objects, allocation ownership or huge-array pointer
wraparound. No 64-bit ABI or additional architecture backend is enabled.

Native stack allocation, nil module calls and signed link selectors are covered
in [jit-frame-audit.md](jit-frame-audit.md).
