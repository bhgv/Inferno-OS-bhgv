# Native fixed power scales and checked native directory packing

## AMD64/ARM64 CVTFX

CVTFX now accepts all 66 exact signed power-of-two scales ±2^k for k=-16..16.
Non-unit scales require a normal source and a normal exact product whose
magnitude fits the existing symmetric integer-conversion bound. Exact ±1
retains the earlier zero/subnormal behavior. Other scales, non-finite values,
non-normal scaling results and boundary errors still use the C handler.

The emitters validate scale fraction/exponent, source/product exponent ranges
and product magnitude with integer instructions before any FP effects. They
then execute the real multiplication, select the half-unit correction from the
actual product sign/zero bits and use the existing truncating conversion.
There is no substitution of bit shifting for the C floating operation, no
runtime helper call, native stack frame or VM-memory write on rejection.
Rounding and standard sticky exceptions remain the C host's FP environment.

The guards exclude underflow/overflow before FP execution; the C fallback
therefore retains those operations' real FP/error effects. In particular,
checking only an adjusted exponent would incorrectly accept a NaN source
under some negative scales, so original and product exponents are checked
independently. Source and scale snapshots remain ahead of all aliased writes.

AMD64 uses volatile rdx/rsi scratch registers; ARM64 uses volatile x2/x15.
The fixed private 64-unit limits are unchanged. Both production sizing emitters
still publish every earlier native path. RV64 is unchanged. This expands one
existing opcode, so counts remain AMD64/ARM64 119 native-path / 54
interpreter-only, RV64+M 103/70, RV64+Zmmul 90/83 and RV64I 87/86.

## Emu and kernel directory packing

Both packdir implementations now capture the complete Sys_Dir from a byte slot
before reading its pointer/scalar fields or converting its strings. They no
longer dereference a potentially unaligned Sys_Dir. The helper's byte-addressed
argument reflects that contract without changing its callers' binary ABI.

Each converted string length is measured once. Allocation and copy sizes use
native ulong widths rather than an int accumulator. Each terminator/header
addition is checked before changing the size; overflow raises exHeap before any
allocation or copy. A representable large size reaches smalloc unchanged and
retains the allocator's normal failure handling. Native string payloads remain
contiguous and metadata assignments retain their existing native conversions.
The input record and its string ownership remain unchanged on every failure.

This removes loss of high size bits and undersized/wrapped allocation requests,
including sums beyond 2/4 GiB on a 64-bit host. It does not change the generated
VM frame/GC pointer ABI or claim full 64-bit VM acceptance.

## Validation

- Physical AMD64 actual-C/native differential: 17,694,720 checks per WORD model,
  35,389,440 total, ASan/UBSan. Every supported exponent/sign is tested. Core
  powers retain the full eight-by-eight source/scale alignment matrix; added
  exponents also cover byte misalignment and all six source/scale/result aliases.
  Tests compare actual C results, all four rounding modes, standard sticky flags
  and complete memory/REG images. Unsupported powers/fractions, zero, infinity,
  signaling NaNs and source/product exponent boundaries are included.
- The native acceptance oracle uses frexp/isnormal and an independently
  evaluated exact floating product rather than copying emitter encodings.
- Unicorn/Capstone real-conversion matrix: 120,928 cases, including ARM64 signed
  exponent arithmetic, guarded rejection, all rounding modes and C FP-status
  oracles for new powers. Existing Unicorn x86 conversion-precision limitations
  are covered by the physical native differential.
- Directory packing: 4,208 cases per emu/os and WORD32/WORD64 configuration,
  16,832 total, ASan/UBSan. All four string fields, shared owners, byte offsets,
  exact contiguous payload/metadata/guards, allocation failures and synthetic
  native-width length limits are covered. A 128-bit size oracle checks the
  production native-width arithmetic; no huge memory buffers are allocated.
- Previous-source replays reproduce a UBSan unaligned field read, a high-bit
  size mismatch at the allocator and an allocation attempt after size overflow.
  PACKDIR_SOURCE selects the source; PACKDIR_PROFILE and PACKDIR_OFFSET isolate
  those scenarios without treating a compilation failure as regression proof.
- All 65 core drivers pass. Complete AMD64 runtime matrices pass ASan/UBSan in
  both WORD models. ARM64 publication/failure/retry/freecode tests, generated
  coverage and the existing Sys status/stat/path fixtures pass.

No Tk, security/crypto or MIPS/SPARC/PowerPC JIT sources are changed. Hardware
ARM64 execution, native block/helper transitions and a coordinated native
64-bit compiler/loader/frame/GC ABI remain separate acceptance work.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/real-to-integer-native.py
PYTHONPATH=../jit-test-deps LEAF64_CASE=realinteger python3 libinterp/tests/leaf64-jit.py
python3 libinterp/tests/sys-packdir.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```
