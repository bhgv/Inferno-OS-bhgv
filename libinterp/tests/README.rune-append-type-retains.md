# Rune concatenation and allocation descriptor lifetime checkpoint

## Instruction availability

Counts are measured by `python3 libinterp/tests/jit-coverage.py` against all
173 active Dis instructions, excluding INOP and the unused IECLR slot.

| Processor/profile | Guarded native paths | Interpreter-only remaining |
| --- | ---: | ---: |
| x64 / AMD64 | 133 | 40 |
| ARM64 / AArch64 | 133 | 40 |
| RISC-V64 + M | 103 | 70 |
| RISC-V64 + Zmmul | 90 | 83 |
| RISC-V64 base integer | 87 | 86 |

This checkpoint broadens the existing ADDC instruction rather than adding an
opcode. WORD32/native-pointer64 and legacy WORD64 have the same availability.
Guarded paths may decline unsupported operands to C. These numbers do not
establish a complete native 64-bit VM/frame/GC ABI port.

## Matching ASCII and Rune concatenation

AMD64 and ARM64 ADDC now accept two matching string representations. Capacity
and combined length are checked in characters; Rune source and destination
offsets are then widened to native byte counts. Up to 128 source characters
are copied, including 512-byte Rune spans. The result retains its signed
representation length. Full 32-bit Rune patterns are copied unchanged.

The source and destination aliases, complete owner counts, both capacities and
overlap direction remain checked before the first write. Self append and
operand slots overwritten by appended bytes retain captured values. Mixed
representations and allocation retain the existing C behavior. In particular,
an allocated empty string has the ASCII representation even when its backing
allocation originally came from newrunes; guards use the actual signed length.

AMD64 combines null/language-nil tests and uses native-width memory comparisons
for reference counts to keep the wider path bounded. ARM64 retains a sign mask
while taking absolute lengths and scales offsets only after the capacity check.
The production 64-unit staging limit is unchanged; the ARM64 path occupies the
full limit. Emission with any insufficient capacity remains write-free.

## Type retention before allocation

heap and heapz now reject C-null, language-nil, dead and saturated Type
descriptors before dereferencing payload metadata or allocating storage.
heaparray, NEWA, NEWAZ and SLICEA preflight both the element Type and Tarray
before acquiring an array header or changing any reference count.

When the element Type is Tarray itself, two references to the same descriptor
are required: one for the array header and one for its elements. A starting
count of INT_MAX-1 must therefore reject before allocation. INT_MAX-2 permits
both increments without signed overflow. Distinct descriptors each reserve
one increment. The existing initarray contract is documented accordingly.

The Dis loader has an additional direct initarray caller. DEFA now checks the
same descriptor capacity before allocating and zeroing the array. An invalid
array Type reference reports a loader diagnostic and unwinds already-loaded
module data without publishing an array. Scalar heap, VM array and loader
paths retain their existing valid behavior and public layouts.

## Verification

All 79 core host drivers pass.

```sh
python3 libinterp/tests/slice-tokenize-suite.py --core
python3 libinterp/tests/string-append-native.py
python3 libinterp/tests/string-append-jit.py
python3 libinterp/tests/type-retains.py
python3 libinterp/tests/dis-loader.py
python3 libinterp/tests/loader-snapshots.py
python3 libinterp/tests/leaf64-runtime.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/type-retains.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/string-append-native.py
```

The expanded native string matrix passes 59,904 production C/RX-JIT cases per
WORD model under UBSan and ASan+UBSan. The independent instruction-emulation
matrix passes 25,756 ARM64 and 25,750 AMD64 memory/write/FP checks, including
matching Rune storage, INT_MIN lengths, mixed-representation fallback and interior operand slots.
Native dispatch/publication and Loader snapshot fixtures also pass.

Type preflight passes 48 constructor/error/boundary cases per WORD model under
UBSan and ASan+UBSan. Rejection preserves complete Type, owner and REG images
with no pool allocation. The parent implementations of all six entry points
fail independently when selected by TYPE_RETAIN_OPERATION. Removing just the
extra Tarray alias reservation reproduces signed overflow in each of the four
array constructors. The parent loader fails its new descriptor rejection case.

The loader fixture also passes 120 truncations, 30,720 byte mutations and its
existing allocation-failure/unwind checks. Both Type preflight and Dis parsing
are registered in the core suite. Selected ASan runs retain stack checking and
emit the sandbox's existing nonlocal-jump runtime warning; complete sanitizer
acceptance for every VM error path is not claimed. Physical ARM64 execution
and complete Inferno boot remain separate acceptance work.
