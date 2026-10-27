# Codec/formatter decoded cells and allocating C publication

This checkpoint hardens the priority AMD64/ARM64 CVTAC, CVTCA, CVTWC, CVTLC
and CVTFC native entries and their real C handlers. ADDC and SLICEC also capture
the original result address across allocating C calls.

Both source and result cell spans are validated before either operand is
consumed. Compound probes give one cell an invalid span and the other operand
an unmapped owner/value, detecting premature dereferences rather than merely
checking the final error. Invalid native paths decline without VM writes; C
reports the appropriate error through the complete-JIT fallback.

Numeric formatting checks backward Heap origins, alignment and the complete
String metadata extent before reading old owners. Generic pointer temporaries
still use C rather than being interpreted as String records. The C handlers
also check the complete output text extent, including its terminating NUL,
before representation changes or payload stores. Both native emitters reject
a terminator at H instead of treating an unchecked last byte as writable.
These arithmetic guards do not prove arbitrary addresses identify mapped storage.

Every successful numeric formatter now publishes the captured output cell after
writing the text and its length, including reuse. This restores an owner slot
which overlaps the formatted payload. AMD64 retains REG until the original
output address is captured in r9; ARM64 saves that address in x5 before writes.
C mirrors the final owner store. The final owner-cell write wins where payload
and output overlap; tests compare the resulting complete image explicitly.

CVTAC/CVTCA capture output before source conversion, allocation or reuse. The
three numeric C formatters, ADDC and SLICEC likewise publish to the original
cell after allocation callbacks borrow R.s/R.m/R.d. Callback register effects
remain visible; no whole-REG restoration or new native ABI is introduced.
Complex UTF/Rune conversion, generic formatting and allocating ownership
management remain in C. Native opcode availability is unchanged.

## Validation

- string-capture-native.py: 159,744 allocating routes, 864 overlapping-output
  reuse routes and 1,218 rejected slot/header/compound routes across WORD32/64
  with 64-bit pointers. All seven operations run through C, native+C and the
  actual complete JIT entries. Production string functions, mem2array and
  nheap are extracted unchanged; bounded fixture pool storage supplies canaries
  and counters. The borrowing callback runs at the actual nheap heapmonitor
  boundary. The complete matrix passes UBSan; successful routes repeat under
  ASan+UBSan/NDEBUG without disabling stack instrumentation.
- string-capture-jit.py under -O: 4,152 AMD64 and 4,115 ARM64 decoded-cell,
  header-origin, compound ordering, VM image, write, preserved-register,
  SP/LR/FP and bounded-publication checks.
- integer-format-jit.py under -O: 666,432 cases and 48 pointer-boundary probes
  per backend. real-format-jit.py: 332,592 cases per backend. Both include byte
  and Rune old buffers and result cells inside the formatted payload. Independent
  images and write oracles require the final pointer-sized owner publication.
  Alignment is now part of the guarded native domain; decoded cells still
  support arbitrary byte alignment. Terminator-at-H probes are rejected.
- integer-format-layouts.py: 702,100 sizing/canary probes per backend;
  real-format-layouts.py: 351,050 per backend. These fixtures now use the actual
  existing 128-unit leaf capacity, including insufficient-capacity checks.
- Existing integer-format-native.py: 704,880 C/RX cases plus Rune/foreign-owner
  matrices. real-format-native.py: 552,960 C/RX cases plus foreign-owner cases.
  byte-string-native.py and string-bytes-native.py each retain 155,136 functional
  C/RX cases with their existing owner, descriptor, UTF/Rune and failure checks.
- Existing byte-string-jit.py: 20,952 AMD64 and 20,934 ARM64 checks; existing
  string-bytes-jit.py: 23,828 AMD64 and 23,806 ARM64 checks. Existing source-span,
  append and slice/length suites pass. Complete dispatch, all-opcode entry
  coverage and ARM64 compiler bridge suites pass. Production string.c compiles
  for both WORD layouts with implicit function declarations treated as errors.
- Fifty-one independent parent probes against 393257e reproduce previous native
  and C cell/header failures, native/C overlapping-publication failures, the
  seven C allocating-publication failures and both native terminator-at-H cases.
  The seven callback probes were also repeated with the real nheap boundary.

Run `python libinterp/tests/string-capture-native.py`. For the successful ASan
repeat set `STRING_CAPTURE_NO_ERRORS=1` and
`CFLAGS="-O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all"`.
Explicit throwing/longjmp paths are isolated under UBSan. C parent replay uses
STRING_CAPTURE_SOURCE and STRING_CAPTURE_SECTION (allocations/aliases/invalid),
with OP/KIND/CALLBACK_ONLY filters prefixed STRING_CAPTURE. Native instruction
replay uses STRING_CAPTURE_EMITTER_DIR/ARCHES/OP/KIND. Formatter image replay
uses FORMAT_EMITTER_DIR, FORMAT_ARCHES, FORMAT_PROFILES and FORMAT_BOUNDARY_ONLY.
The native/RX fixture requires AMD64; missing emulator dependencies fail.

AMD64 and ARM64 retain 173/173 complete JIT+C entries and 166/173 guarded native
paths (95.95%). Opcode entry counts do not describe every operand domain or a
finished 64-bit port. Physical ARM64 execution and full Inferno boot remain
unverified. RV64 is still deferred; excluded backends and Tk/security/crypto
are unchanged.
