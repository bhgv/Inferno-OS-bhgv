# Native byte-prefix copy and array-view lifetime checkpoint

This records the initial prefix-only path. The subsequent
[offset-slice/channel checkpoint](README.offset-slices-channels.md) removes the
index-zero restriction while preserving the 64-unit emission limit.

AMD64 and ARM64 now publish ISLICELA for destination index zero, non-pointer
byte elements and source lengths up to 128. This adds one opcode ID per backend:
128 native paths and 45 interpreter-only commands out of 173 active commands.
INOP is invalid and IECLR is unused. RV64 counts remain 103/70 with M, 90/83
with Zmmul, and 87/86 with the base integer ISA. WORD32/native-pointer64 and
legacy WORD64 have the same counts.

## Native subset

An absent source (H or C null) succeeds without accessing array metadata or
changing storage, following C. For a real source, the destination must exist,
its index must be zero, its nonnegative length must cover the nonnegative source
length, and the source length must fit the native limit. A validated empty
prefix returns before type, map, data or reference access.

For nonempty copies, the destination descriptor must have size one and np zero.
Both data pointers must be real, and each complete native address span must fit
without unsigned wrap. Source and destination array reference counts are
untouched, as in C's non-pointer branch. Other indexes, descriptors and lengths
retain the platform C helpers. Source type metadata is not newly constrained:
the C instruction uses the destination element descriptor for both spans.

A single signed-stride byte loop implements both memmove overlap directions.
AMD64 never changes DF. All guards precede guest stores, and no accepted path
allocates or calls a helper. Both backends retain the 64-unit private emission
limit and ABI caller-saved integer registers. FP controls/status remain intact.

## Interpreter corrections

C slicea now validates a real backing root, a nonzero current view/root owner
count, and a backing count with room for an additional reference when retaining
an existing backing root. These checks occur before allocating the new Array,
changing type/reference counts or publishing a result. The new view's root is
initialized before publication. A first view still transfers its old owning
array into the new root without an unnecessary increment; a saturated count in
that transfer does not wrap and remains permitted.

C slicela validates zero/maximum source reference counts before its temporary
retain in the mapped-owner branch. No destination bytes or mapped owners can
change before this lifetime check. Empty copies, zero-sized elements and the
non-pointer branch keep their earlier early-return/borrowing behavior.

## Evidence

- slice-copy-native.py executes the production AMD64 adapter and actual
  arrayspan/slicela implementations against independent whole byte images and
  libc memmove. There are 1198080 cases per WORD model, 2396160 total, under
  ASan/UBSan. The matrix covers every source/index/destination alignment,
  overlapping directions, decoded-operand aliases, element sizes zero/one/two,
  zero-bit mapped fallback traversal, nonzero offsets and lengths through 255.
  The C/harness are instrumented; generated code itself is not.
- slice-copy-jit.py executes actual AMD64/ARM64 emitter bytes with Unicorn and
  checks instruction boundaries with Capstone. There are 53014 combined checks
  of full memory images above 4 GiB, all alignments, aliases/overlap, absent
  owners, negative and wide WORD metadata, destination bounds, invalid types,
  maps, null/H data and address-wrap rejection. Guest write hooks verify empty
  and declined cases perform no stores. FP controls and status are preserved.
- The expanded mapped-copy-layouts.py matrix checks all three recent copy
  opcodes, large layouts and every capacity from zero through 64 under ASan/
  UBSan: 106080 cases per architecture, 212160 total. Output canaries must
  survive every declined/undersized emission.
- heap-bounds.py with HEAP_CASE=slice-owners runs 4096 cases per WORD model,
  8192 total, under ASan/UBSan. Null roots, dead views, zero/MAX backing owners,
  empty views and mapped-copy temporary counts must fail before any pool call,
  mark, byte change or owner publication. All three operand alignments are
  varied independently.
- The core suite passes 71 host drivers. Actual AMD64 publication/execution
  passes ASan/UBSan in both WORD models. ARM64 publication passes both models;
  RV64 publication still confirms ISLICELA is interpreter-only. The shared
  failure/protection/retry/unmap fixture now contains 75 instructions.
- Selecting the parent interpreter with HEAP_XEC_SOURCE reproduces null-root
  failure, backing-count wrap/invalid-owner acceptance and temporary count wrap.
  SLICE_OWNER_MODE=0/2/6 isolates these cases; mode 7 isolates a dead view.

```
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' python3 libinterp/tests/slice-copy-native.py
PYTHONPATH=../jit-test-deps python3 libinterp/tests/slice-copy-jit.py
python3 libinterp/tests/mapped-copy-layouts.py
CFLAGS='-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all' HEAP_CASE=slice-owners python3 libinterp/tests/heap-bounds.py
python3 libinterp/tests/slice-tokenize-suite.py --core
```

SLICE_COPY_SOURCE selects alternate C slice-copy implementations. Physical
ARM64 execution and complete VM/frame/GC/native-block ABI acceptance remain
separate. Opcode availability is partial and guarded. Tk, security/crypto and
MIPS/SPARC/PowerPC JIT sources are outside this checkpoint.
