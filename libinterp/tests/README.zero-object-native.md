# NEWZ/MNEWZ native reuse and constructor map bounds

For the subsequent NEW extension and expanded regression counts, see
[README.raw-record-arrays.md](README.raw-record-arrays.md).

AMD64 and ARM64 now emit INEWZ and IMNEWZ for standard pointer-free objects
with payload sizes from zero through 256 bytes. Both WORD32/native-pointer and
legacy WORD64 models resolve the full index through the current Module.type
table; a stale Modlink.type cache is never used. This raises each backend from
144 to 146 supported active opcode IDs, leaving 27 interpreter-only IDs.
RV64 availability is unchanged. Native availability remains conditional on
operands; these counts do not imply complete native execution for every opcode.

A reusable owner has exactly one reference, the requested descriptor, no map
bytes, and the exact freeheap and markheap callbacks. Pointer-free custom types
can still own native resources, so checking np alone would leak those resources.
Descriptor and owner references must be live. Reuse does not allocate, retain,
release, recolor, or change collector state. A saturated positive descriptor
reference count is acceptable on reuse because no retain occurs. Native code
uses only caller-saved integer registers and preserves floating-point state.

The C interpreter shares the same callback and ownership rules, allowing larger
reusable payloads. Other cases allocate through heapz, complete initialization,
publish the new owner and then release the old owner. Allocation failures leave
the previous destination intact. Index and module operands are captured before
clearing a potentially overlapping payload; publication restores an overlapping
result slot. Zero-byte objects require no payload access.

Module lookup checks nil/sentinel pointers, complete native address spans,
table-cell multiplication/addition, index bounds and descriptor size before
reading the associated field. These checks also protect other interpreter users
of moduletype. They detect arithmetic wrapping; arbitrary unmapped pointers
remain outside the valid VM object contract.

heap and heapz now reject negative/excessive map lengths and bitmap bits which
would initialize an incomplete or out-of-range native pointer slot. Validation
occurs before allocation or descriptor retention. Both constructors and
heaparray and newmp also reject wrapping map spans. Previously heap/heapz could allocate
an undersized payload and let initmem write past it. The new constructor oracle
fails against the parent heap.c and passes against the repaired implementation.

Validation drivers:

- newzero-native.py extracts production C and compares real executable AMD64
  leaves with C for 168,960 cases across both WORD models, plus 102,960 direct
  constructor map cases and 30 module-span error cases. It covers custom native
  finalizers, custom markers, mapped and shared owners, stale module caches,
  aliases, allocation failure, reference counts, complete images and FP state.
- newzero-jit.py runs production AMD64/ARM64 emitted instructions in Unicorn:
  103,114 cases including high native addresses, saturated references, full-width
  callback identities, zero payloads, output capacities and preserved registers.
- newzero-layouts.py sweeps private layouts and output capacities under ASan and
  UBSan: 1,755,250 publication/canary checks per backend. Rejected emissions must
  leave caller buffers untouched.
- ZERO_NO_ERRORS=1 with ASan/UBSan exercises successful constructor/owner paths;
  the full error matrix runs with UBSan and NDEBUG. The container's ASan stack
  discovery emits environment warnings, so explicit resource counts accompany it.

The older general leaf fixture also receives a sufficient instruction budget
for bounded list scans; its previous 4,000-instruction stop could leave a scan
unfinished and mistake a live address register for the returned status. The
focused list checkpoint passes on AMD64, ARM64 and RV64 (1,438 checks).

Run the native driver directly, or the core checkpoint with
`python3 libinterp/tests/slice-tokenize-suite.py --core`. Emission drivers are
included with `--jit`. ARM64 evidence is emulated; physical target acceptance
and complete Inferno runtime acceptance remain separate.
