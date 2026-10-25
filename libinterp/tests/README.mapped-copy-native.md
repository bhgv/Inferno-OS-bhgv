# Native typed-record copy checkpoint

AMD64 and ARM64 now publish IMOVMP and IHEADMP leaves for pointer-free records
up to 256 bytes. The original checkpoint added two opcode IDs per backend. Current totals are
138 guarded native paths and 35 interpreter-only commands out of 173 active commands. INOP is invalid and
IECLR is unused. RV64 counts remain 103/70 with M, 90/83 with Zmmul, and 87/86
with its base integer ISA. Both WORD32/native-pointer64 and legacy WORD64
configurations give these counts.

## Native contract

Each invocation resolves R.M, its real Module, the current Module.type vector,
a signed WORD index, and the selected Type. It never reads the stale cached
Modlink.type vector. Native offsets come from the compiler layout adapter.
The descriptor's size and pointer-map length are native C ints, independently
of the language WORD width. Pointer-vector arithmetic is bounded before lookup.

For sizes 1 through 256, only np==0 is accepted. Both overlap directions use a
single byte loop with a signed native stride, preserving memmove byte results
without setting the AMD64 direction flag or calling helpers. Larger records and
records containing pointers retain the existing C ownership/memmove handlers.

IHEADMP validates the list before resolving type metadata. A nonempty native
copy checks its heap reference count is neither zero nor maximum, then publishes
the payload address into R.s as C does. Since the accepted path has no mapped
owners, helper calls or allocation, the C temporary retain/release can be omitted.
The source count and marking remain unchanged. A zero-size record returns before
source/destination data access, map checks or temporary lifetime retention; its
R.s remains unchanged. A list identity is still required for IHEADMP.

All decline checks precede VM stores. The original 64-unit private buffer limit
is retained. AMD64 uses two nearby rejection returns to bound short branches;
both backends validate staging capacity before patching internal branch slots.
Only ABI caller-saved integer registers are used; FP state is preserved.

## Interpreter fixes

The shared moduletype resolver now rejects a Module.type vector equal to H,
just as it already rejects a C-null vector. Previously an otherwise bounded
lookup could overflow pointer arithmetic through H and dereference invalid
memory. This protects all interpreter consumers using the shared resolver.

C headmp now checks zero/maximum source reference counts before acquiring its
temporary source owner. Invalid lifetime state raises exHeap before any retain,
operand change or copy. The original zero-size early return remains intact.

## Evidence

- mapped-copy-native.py executes the production AMD64 adapter and actual C
  moduletype/listhead/movmp/headmp bodies against independent full byte-image
  expectations and libc memmove. There are 226576 cases per WORD model, 453152
  total, under ASan/UBSan: all source/destination/index alignments, overlap in
  both directions, operand aliases, moving live tables, stale link caches,
  sizes through 257, metadata failures, empty mapped records and reference
  limits. C and the harness are instrumented; generated code itself is not.
- mapped-copy-jit.py executes production AMD64/ARM64 bytes with Unicorn and
  checks instruction boundaries with Capstone (59620 cases per architecture,
  119240 total). It compares complete memory
  images above 4 GiB and verifies with guest write hooks that zero-size and
  declined invocations make no stores. It checks invalid module/table/type
  identities, signed index/count boundaries, pointer-wrap rejection, mapped
  records, list/ref failures, malformed layouts, zero-size invalid data pointers,
  aliases, overlap and unchanged FP controls/status.
- mapped-copy-layouts.py passes 106080 layout/capacity probes per architecture,
  212160 total, under ASan/UBSan. Large native offsets and every capacity from
  zero through 64 must preserve output canaries or publish a complete leaf.
- The core suite now includes 91 host drivers. The publication fixture now contains
  all new opcodes in its 74-instruction module and preserves the actual
  allocation/protection failure, retry and unmap checks. Actual AMD64 execution
  passes ASan/UBSan in both WORD models; ARM64 publication passes both models;
  RV64 publication confirms the new paths are absent.
- Replaying the parent interpreter with MAPPED_ERROR_ONLY=1 and
  MAPPED_ERROR_CASE=6 reproduces the H-vector pointer arithmetic fault; case 17
  reproduces temporary reference wrap before destruction. MAPPED_COPY_SOURCE
  selects the alternate interpreter source. Case 16 isolates the zero count.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/mapped-copy-native.py
PYTHONPATH=../jit-test-deps python3 libinterp/tests/mapped-copy-jit.py
python3 libinterp/tests/mapped-copy-layouts.py
python3 libinterp/tests/jit-coverage.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

Physical ARM64 execution and full VM/native-block/frame/GC ABI acceptance remain
separate. Opcode availability is guarded and partial. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are outside this checkpoint.
